#include "queue.h"

#include <gtest/gtest.h>
#include <pthread.h>

class QueueTest : public ::testing::Test {
      protected:
        queue_t q;

        void SetUp() override {
                memset(&q, 0, sizeof(q));
                ASSERT_EQ(init_queue(&q), 0);
        }

        void TearDown() override {
                if (atomic_load(&q.init)) {
                        deinit_queue(&q);
                }
        }
};

TEST_F(QueueTest, DoubleInitFails) { EXPECT_EQ(init_queue(&q), -1); }

TEST_F(QueueTest, DeinitOnUninitializedFails) {
        deinit_queue(&q);
        EXPECT_EQ(deinit_queue(&q), -1);
}

TEST_F(QueueTest, EnqueueDequeueSingleItem) {
        int val = 42;
        ASSERT_EQ(enqueue(&q, &val), 0);
        // dequeue would block if empty — it won't here
        // spin dequeue in a thread to be safe
        void *result = nullptr;

        pthread_t t;
        pthread_create(&t, nullptr, [](void *arg) -> void * { return dequeue((queue_t *)arg); }, &q);
        pthread_join(t, &result);

        EXPECT_EQ(result, &val);
        EXPECT_EQ(*(int *)result, 42);
}

TEST_F(QueueTest, FIFOOrdering) {
        int vals[4] = {1, 2, 3, 4};
        for (auto &v : vals)
                ASSERT_EQ(enqueue(&q, &v), 0);

        for (auto &v : vals) {
                void *result = nullptr;
                pthread_t t;
                pthread_create(&t, nullptr, [](void *arg) -> void * { return dequeue((queue_t *)arg); }, &q);
                pthread_join(t, &result);
                EXPECT_EQ(result, &v);
        }
}

TEST_F(QueueTest, FullQueueRejectsEnqueue) {
        int dummy = 0;
        for (int i = 0; i < QUEUE_SIZE; i++)
                ASSERT_EQ(enqueue(&q, &dummy), 0);

        EXPECT_EQ(enqueue(&q, &dummy), -1);
}

TEST_F(QueueTest, EnqueueAfterDequeueOnFullQueue) {
        int dummy = 0;
        for (int i = 0; i < QUEUE_SIZE; i++)
                ASSERT_EQ(enqueue(&q, &dummy), 0);

        // drain one slot
        void *r = nullptr;
        pthread_t t;
        pthread_create(&t, nullptr, [](void *arg) -> void * { return dequeue((queue_t *)arg); }, &q);
        pthread_join(t, &r);

        // now one slot free
        EXPECT_EQ(enqueue(&q, &dummy), 0);
}

TEST_F(QueueTest, ShutdownWakesBlockedWorker) {
        // Worker will block — queue is empty
        void *result = (void *)0xDEAD; // sentinel to detect change
        pthread_t t;
        pthread_create(&t, nullptr, [](void *arg) -> void * { return dequeue((queue_t *)arg); }, &q);

        usleep(10000); // give worker time to block in cond_wait
        deinit_queue(&q);

        pthread_join(t, &result);
        EXPECT_EQ(result, nullptr); // dequeue must return NULL on shutdown
}

TEST_F(QueueTest, EnqueueRejectedAfterShutdown) {
        int val = 1;
        deinit_queue(&q);
        EXPECT_EQ(enqueue(&q, &val), -1);
}

struct ConcurrentArgs {
        queue_t *q;
        int count;
        int received;
};

TEST_F(QueueTest, MultipleProducersOneConsumer) {
        constexpr int N = 8;
        int vals[N] = {};

        pthread_t producers[2];
        for (int p = 0; p < 2; p++) {
                pthread_create(
                    &producers[p], nullptr,
                    [](void *arg) -> void * {
                            auto *data = (std::pair<queue_t *, int *> *)arg;
                            for (int i = 0; i < N / 2; i++)
                                    enqueue(data->first, data->second + i);
                            return nullptr;
                    },
                    new std::pair<queue_t *, int *>(&q, vals + p * (N / 2)));
        }

        // 1 consumer collects all
        int received = 0;
        pthread_t consumer;
        pthread_create(
            &consumer, nullptr,
            [](void *arg) -> void * {
                    auto *data = (std::pair<queue_t *, int *> *)arg;
                    while (*data->second < N) {
                            void *r = dequeue(data->first);
                            if (r == nullptr)
                                    break;
                            (*data->second)++;
                    }
                    return nullptr;
            },
            new std::pair<queue_t *, int *>(&q, &received));

        for (auto &p : producers)
                pthread_join(p, nullptr);
        deinit_queue(&q); // wakes consumer when done
        pthread_join(consumer, nullptr);

        EXPECT_EQ(received, N);
}
