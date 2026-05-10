#include "queue.h"

#include <atomic>
#include <gtest/gtest.h>
#include <pthread.h>
#include <unistd.h>

static void *dequeue_thread(void *arg) { return dequeue((queue_t *)arg); }

struct ProducerArg {
        queue_t *q;
        void **items;
        int count;
};

static void *producer_thread(void *arg) {
        auto *a = (ProducerArg *)arg;
        for (int i = 0; i < a->count; i++) {
                while (enqueue(a->q, a->items[i]) != 0)
                        usleep(100);
        }
        return nullptr;
}

struct ConsumerArg {
        queue_t *q;
        std::atomic<int> *received;
};

static void *consumer_thread(void *arg) {
        auto *a = (ConsumerArg *)arg;
        while (true) {
                void *r = dequeue(a->q);
                if (!r)
                        break; 
                a->received->fetch_add(1, std::memory_order_relaxed);
        }
        return nullptr;
}

class QueueTest : public ::testing::Test {
      protected:
        queue_t q;
        bool destroyed = false;

        void SetUp() override {
                memset(&q, 0, sizeof(q));
                init_queue(&q);
        }

        void TearDown() override {
                if (!destroyed) {
                        shutdown_queue(&q);
                        destroy_queue(&q);
                }
        }

        void FullTeardown() {
                shutdown_queue(&q);
                destroy_queue(&q);
                destroyed = true;
        }
};

TEST_F(QueueTest, InitSetsCleanState) {
        EXPECT_EQ(q.count, 0);
        EXPECT_EQ(q.head, 0);
        EXPECT_EQ(q.tail, 0);
        EXPECT_FALSE(q.shutdown);
}

TEST_F(QueueTest, EnqueueSingleItemSucceeds) {
        int val = 42;
        EXPECT_EQ(enqueue(&q, &val), 0);
        EXPECT_EQ(q.count, 1);
}

TEST_F(QueueTest, EnqueueFillsQueueToCapacity) {
        int dummy = 0;
        for (int i = 0; i < QUEUE_SIZE; i++)
                ASSERT_EQ(enqueue(&q, &dummy), 0);
        EXPECT_EQ(q.count, QUEUE_SIZE);
}

TEST_F(QueueTest, EnqueueRejectsWhenFull) {
        int dummy = 0;
        for (int i = 0; i < QUEUE_SIZE; i++)
                ASSERT_EQ(enqueue(&q, &dummy), 0);

        EXPECT_EQ(enqueue(&q, &dummy), -1);
        EXPECT_EQ(q.count, QUEUE_SIZE);
}

TEST_F(QueueTest, EnqueueRejectsAfterShutdown) {
        int val = 1;
        shutdown_queue(&q);
        EXPECT_EQ(enqueue(&q, &val), -1);
        EXPECT_EQ(q.count, 0);
        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, EnqueueSucceedsAfterSlotFreed) {
        int dummy = 0;
        for (int i = 0; i < QUEUE_SIZE; i++)
                ASSERT_EQ(enqueue(&q, &dummy), 0);

        pthread_t t;
        void *r;
        pthread_create(&t, nullptr, dequeue_thread, &q);
        pthread_join(t, &r);

        EXPECT_EQ(enqueue(&q, &dummy), 0);
        EXPECT_EQ(q.count, QUEUE_SIZE);
}

TEST_F(QueueTest, DequeueReturnsSingleItem) {
        int val = 42;
        ASSERT_EQ(enqueue(&q, &val), 0);

        pthread_t t;
        void *result;
        pthread_create(&t, nullptr, dequeue_thread, &q);
        pthread_join(t, &result);

        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result, &val);
        EXPECT_EQ(*(int *)result, 42);
}

TEST_F(QueueTest, DequeueBlocksUntilItemArrives) {
        int val = 99;

        pthread_t t;
        void *result;
        pthread_create(&t, nullptr, dequeue_thread, &q);

        usleep(20000);
        enqueue(&q, &val);
        pthread_join(t, &result);

        ASSERT_NE(result, nullptr);
        EXPECT_EQ(*(int *)result, 99);
}

TEST_F(QueueTest, DequeueReturnsFIFOOrder) {
        int vals[4] = {10, 20, 30, 40};
        for (auto &v : vals)
                ASSERT_EQ(enqueue(&q, &v), 0);

        for (auto &v : vals) {
                pthread_t t;
                void *result;
                pthread_create(&t, nullptr, dequeue_thread, &q);
                pthread_join(t, &result);
                EXPECT_EQ(result, &v);
        }
}

TEST_F(QueueTest, DequeueDecrementsCount) {
        int a = 1, b = 2;
        enqueue(&q, &a);
        enqueue(&q, &b);
        EXPECT_EQ(q.count, 2);

        pthread_t t;
        void *r;
        pthread_create(&t, nullptr, dequeue_thread, &q);
        pthread_join(t, &r);
        EXPECT_EQ(q.count, 1);

        pthread_create(&t, nullptr, dequeue_thread, &q);
        pthread_join(t, &r);
        EXPECT_EQ(q.count, 0);
}

TEST_F(QueueTest, DequeueReturnsNullWhenShutdownAndEmpty) {
        void *result = (void *)0xDEAD;
        pthread_t t;
        pthread_create(&t, nullptr, dequeue_thread, &q);

        usleep(20000);
        shutdown_queue(&q);
        pthread_join(t, &result);

        EXPECT_EQ(result, nullptr);
        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, DequeueDeliversRemainingItemsAfterShutdown) {
        int vals[3] = {1, 2, 3};
        for (auto &v : vals)
                ASSERT_EQ(enqueue(&q, &v), 0);

        shutdown_queue(&q);
        for (auto &v : vals) {
                pthread_t t;
                void *result;
                pthread_create(&t, nullptr, dequeue_thread, &q);
                pthread_join(t, &result);
                EXPECT_EQ(result, &v);
        }

        pthread_t t;
        void *result;
        pthread_create(&t, nullptr, dequeue_thread, &q);
        pthread_join(t, &result);
        EXPECT_EQ(result, nullptr);

        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, CircularBufferWrapAround) {
        int first[QUEUE_SIZE / 2], second[QUEUE_SIZE / 2];
        for (int i = 0; i < QUEUE_SIZE / 2; i++) {
                first[i] = i;
        }
        for (int i = 0; i < QUEUE_SIZE / 2; i++) {
                second[i] = i + 100;
        }

        for (int i = 0; i < QUEUE_SIZE / 2; i++)
                ASSERT_EQ(enqueue(&q, &first[i]), 0);

        for (int i = 0; i < QUEUE_SIZE / 2; i++) {
                pthread_t t;
                void *r;
                pthread_create(&t, nullptr, dequeue_thread, &q);
                pthread_join(t, &r);
                EXPECT_EQ(*(int *)r, i);
        }
        EXPECT_EQ(q.count, 0);

        for (int i = 0; i < QUEUE_SIZE / 2; i++)
                ASSERT_EQ(enqueue(&q, &second[i]), 0);

        for (int i = 0; i < QUEUE_SIZE / 2; i++) {
                pthread_t t;
                void *r;
                pthread_create(&t, nullptr, dequeue_thread, &q);
                pthread_join(t, &r);
                EXPECT_EQ(*(int *)r, i + 100);
        }
}

TEST_F(QueueTest, ShutdownSetsFlag) {
        EXPECT_FALSE(q.shutdown);
        shutdown_queue(&q);
        EXPECT_TRUE(q.shutdown);
        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, ShutdownCalledTwiceDoesNotHangOrCrash) {
        shutdown_queue(&q);
        shutdown_queue(&q);
        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, ShutdownWakesAllBlockedWorkers) {
        constexpr int N = 4;
        pthread_t threads[N];

        for (int i = 0; i < N; i++)
                pthread_create(&threads[i], nullptr, dequeue_thread, &q);

        usleep(20000);
        shutdown_queue(&q);

        void *results[N];
        for (int i = 0; i < N; i++)
                pthread_join(threads[i], &results[i]);

        for (int i = 0; i < N; i++)
                EXPECT_EQ(results[i], nullptr);

        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, OneProducerMultipleConsumers) {
        constexpr int NUM_CONSUMERS = 4;
        constexpr int NUM_TASKS = 64;

        int vals[NUM_TASKS];
        void *ptrs[NUM_TASKS];
        for (int i = 0; i < NUM_TASKS; i++) {
                vals[i] = i;
                ptrs[i] = &vals[i];
        }

        std::atomic<int> received{0};
        ConsumerArg carg{&q, &received};
        pthread_t consumers[NUM_CONSUMERS];
        for (int i = 0; i < NUM_CONSUMERS; i++)
                pthread_create(&consumers[i], nullptr, consumer_thread, &carg);

        ProducerArg parg{&q, ptrs, NUM_TASKS};
        pthread_t producer;
        pthread_create(&producer, nullptr, producer_thread, &parg);
        pthread_join(producer, nullptr);

        usleep(50000);
        shutdown_queue(&q);
        for (auto &c : consumers)
                pthread_join(c, nullptr);

        EXPECT_EQ(received.load(), NUM_TASKS);
        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, MultipleProducersOneConsumer) {
        constexpr int NUM_PRODUCERS = 4;
        constexpr int TASKS_PER_PROD = 16;
        constexpr int TOTAL = NUM_PRODUCERS * TASKS_PER_PROD;

        int vals[NUM_PRODUCERS][TASKS_PER_PROD];
        void *ptrs[NUM_PRODUCERS][TASKS_PER_PROD];
        for (int p = 0; p < NUM_PRODUCERS; p++)
                for (int i = 0; i < TASKS_PER_PROD; i++) {
                        vals[p][i] = p * 100 + i;
                        ptrs[p][i] = &vals[p][i];
                }

        pthread_t producers[NUM_PRODUCERS];
        ProducerArg pargs[NUM_PRODUCERS];
        for (int p = 0; p < NUM_PRODUCERS; p++) {
                pargs[p] = {&q, ptrs[p], TASKS_PER_PROD};
                pthread_create(&producers[p], nullptr, producer_thread, &pargs[p]);
        }

        std::atomic<int> received{0};
        ConsumerArg carg{&q, &received};
        pthread_t consumer;
        pthread_create(&consumer, nullptr, consumer_thread, &carg);

        for (auto &p : producers)
                pthread_join(p, nullptr);

        usleep(50000);
        shutdown_queue(&q);
        pthread_join(consumer, nullptr);

        EXPECT_EQ(received.load(), TOTAL);
        destroy_queue(&q);
        destroyed = true;
}

TEST_F(QueueTest, MultipleProducersMultipleConsumers) {
        constexpr int NUM_PRODUCERS = 4;
        constexpr int NUM_CONSUMERS = 4;
        constexpr int TASKS_PER_PROD = 32;
        constexpr int TOTAL = NUM_PRODUCERS * TASKS_PER_PROD;

        int vals[NUM_PRODUCERS][TASKS_PER_PROD];
        void *ptrs[NUM_PRODUCERS][TASKS_PER_PROD];
        for (int p = 0; p < NUM_PRODUCERS; p++)
                for (int i = 0; i < TASKS_PER_PROD; i++) {
                        vals[p][i] = p * 1000 + i;
                        ptrs[p][i] = &vals[p][i];
                }

        pthread_t producers[NUM_PRODUCERS];
        ProducerArg pargs[NUM_PRODUCERS];
        for (int p = 0; p < NUM_PRODUCERS; p++) {
                pargs[p] = {&q, ptrs[p], TASKS_PER_PROD};
                pthread_create(&producers[p], nullptr, producer_thread, &pargs[p]);
        }

        std::atomic<int> received{0};
        ConsumerArg carg{&q, &received};
        pthread_t consumers[NUM_CONSUMERS];
        for (int i = 0; i < NUM_CONSUMERS; i++)
                pthread_create(&consumers[i], nullptr, consumer_thread, &carg);

        for (auto &p : producers)
                pthread_join(p, nullptr);

        usleep(50000);
        shutdown_queue(&q);
        for (auto &c : consumers)
                pthread_join(c, nullptr);

        EXPECT_EQ(received.load(), TOTAL);
        destroy_queue(&q);
        destroyed = true;
}
