#include "unity.h"
#include "ring_buffer.h"

#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(x[0]))

static struct {
    int16_t lock_count;
} g_state = { 0 };

void setUp(void)
{
    g_state.lock_count = 0;
}

bool global_lock(void)
{
    bool success;
    if (g_state.lock_count > 0)
    {
        success = false;
    }
    else
    {
        g_state.lock_count++;
        success = true;
    }
    return success;
}

void global_unlock(void)
{
    TEST_ASSERT_GREATER_THAN(0, g_state.lock_count);
    g_state.lock_count--;
}

template<typename T, size_t capacity>
RingBuffer<T, capacity, false, global_lock, global_unlock> make_buffer(size_t offset)
{
    auto rb = RingBuffer<T, capacity, false, global_lock, global_unlock>();
    rb.testonly_advance_pointers(offset);
    return rb;
}

template<typename T, size_t buffer_size>
void subtest_put_and_get_one(size_t offset)
{
    T arbitrary = (T)-172983;

    auto rb = make_buffer<T, buffer_size>(offset);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, rb.put(arbitrary));

    auto result = rb.get();
    TEST_ASSERT_EQUAL(arbitrary, result.value);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
}

void test_put_and_get_one_with_wraparound(void)
{
    subtest_put_and_get_one<int32_t, 16>(0);
    subtest_put_and_get_one<int32_t, 16>(4);
    subtest_put_and_get_one<int32_t, 16>(8);
    subtest_put_and_get_one<int32_t, 16>(12);
    subtest_put_and_get_one<int32_t, 16>(16);
}

template<typename T, size_t buffer_size>
void subtest_put_and_get_two(size_t offset)
{
    T first = (T)-172983;
    T second = (T)0x7eadbeef;

    auto rb = make_buffer<T, buffer_size>(offset);

    TEST_ASSERT_EQUAL(RB_ERROR_NONE, rb.put(first));
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, rb.put(second));

    auto result = rb.get();
    TEST_ASSERT_EQUAL(first, result.value);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);

    result = rb.get();
    TEST_ASSERT_EQUAL(second, result.value);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
}

void test_put_and_get_two()
{
    subtest_put_and_get_two<int32_t, 16>(0);
    subtest_put_and_get_two<int32_t, 16>(4);
    subtest_put_and_get_two<int32_t, 16>(8);
    subtest_put_and_get_two<int32_t, 16>(12);
    subtest_put_and_get_two<int32_t, 16>(16);
}

template<typename T, size_t buffer_size>
void subtest_put_many_and_get_many(size_t offset)
{
    T data[buffer_size];
    auto rb = make_buffer<T, buffer_size>(offset);

    for (size_t i = 0; i < buffer_size; i++)
    {
        data[i] = i * 1000;
    }

    for (size_t i = 0; i < buffer_size; i++)
    {
        TEST_ASSERT_EQUAL(RB_ERROR_NONE, rb.put(data[i]));
    }

    for (size_t i = 0; i < buffer_size; i++)
    {
        auto result = rb.get();
        TEST_ASSERT_EQUAL(data[i], result.value);
        TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    }
}

void test_put_many_and_get_many(void)
{
    subtest_put_many_and_get_many<int32_t, 128>(0);
    subtest_put_many_and_get_many<int32_t, 128>(99);
}

template<typename T, size_t buffer_size>
void subtest_put_array_and_get_many(size_t offset)
{
    T data[buffer_size];
    auto rb = make_buffer<T, buffer_size>(offset);

    for (size_t i = 0; i < buffer_size; i++)
    {
        data[i] = i * 1000;
    }

    auto result = rb.put(data, buffer_size /2);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/2, result.value);

    for (size_t i = 0; i < buffer_size/2; i++)
    {
        auto result = rb.get();
        TEST_ASSERT_EQUAL(data[i], result.value);
        TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    }
}

void test_put_array_and_get_many(void)
{
    subtest_put_array_and_get_many<int32_t, 128>(0);
    subtest_put_array_and_get_many<int32_t, 128>(99);
}

template<typename T, size_t buffer_size>
void subtest_put_many_and_get_array(size_t offset)
{
    T data[buffer_size];
    auto rb = make_buffer<T, buffer_size>(offset);

    for (size_t i = 0; i < buffer_size; i++)
    {
        data[i] = i * 1000;
    }

    for (size_t i = 0; i < buffer_size/2; i++)
    {
        TEST_ASSERT_EQUAL(RB_ERROR_NONE, rb.put(data[i]));
    }

    T received[buffer_size/2];
    auto result = rb.get(received, buffer_size/2);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/2, result.value);
    for (size_t i = 0; i < buffer_size/2; i++)
    {
        TEST_ASSERT_EQUAL(data[i], received[i]);
    }
}

void test_put_many_and_get_array(void)
{
    subtest_put_many_and_get_array<int32_t, 128>(64);
    subtest_put_many_and_get_array<int32_t, 128>(0);
}

template<typename T, size_t buffer_size>
void subtest_diagnostics(size_t offset)
{
    auto rb = make_buffer<T, buffer_size>(offset);

    // Initial
    auto result = rb.is_empty();
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_TRUE(result.value);
    TEST_ASSERT_FALSE(rb.is_full());
    TEST_ASSERT_EQUAL(0, rb.level().value);
    TEST_ASSERT_EQUAL(buffer_size, rb.available().value);

    for (size_t i = 0; i < buffer_size-1; i++)
    {
        rb.put(1234);
        TEST_ASSERT_FALSE(rb.is_empty().value);
        TEST_ASSERT_FALSE(rb.is_full());
        TEST_ASSERT_EQUAL(i+1, rb.level().value);
        TEST_ASSERT_EQUAL(buffer_size-i-1, rb.available().value);
    }

    rb.put(1234);
    TEST_ASSERT_EQUAL(buffer_size, rb.level().value);
    TEST_ASSERT_EQUAL(0, rb.available().value);
    TEST_ASSERT_FALSE(rb.is_empty().value);
    TEST_ASSERT_TRUE(rb.is_full());
}

void test_diagnostics(void)
{
    subtest_diagnostics<int32_t, 16>(0);
    subtest_diagnostics<int32_t, 16>(4);
    subtest_diagnostics<int32_t, 16>(8);
    subtest_diagnostics<int32_t, 16>(12);
    subtest_diagnostics<int32_t, 16>(16);
}

template<typename T, size_t buffer_size>
void subtest_get_from_empty(size_t offset)
{
    auto rb = make_buffer<T, buffer_size>(offset);
    auto result = rb.get();
    TEST_ASSERT_EQUAL(RB_ERROR_ILLEGAL, result.error);
}

void test_get_from_empty(void)
{
    subtest_get_from_empty<int32_t, 16>(0);
    subtest_get_from_empty<int32_t, 16>(4);
    subtest_get_from_empty<int32_t, 16>(8);
    subtest_get_from_empty<int32_t, 16>(12);
}

template<typename T, size_t buffer_size>
void subtest_get_many_from_empty(size_t offset)
{
    auto rb = make_buffer<T, buffer_size>(offset);
    int32_t data[4];
    auto result = rb.get(data, ARRAY_LENGTH(data));
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(0, result.value);
}

void test_get_many_from_empty(void)
{
    subtest_get_many_from_empty<int32_t, 16>(0);
    subtest_get_many_from_empty<int32_t, 16>(4);
    subtest_get_many_from_empty<int32_t, 16>(8);
    subtest_get_many_from_empty<int32_t, 16>(12);
}

template<typename T, size_t buffer_size>
void subtest_put_to_full(size_t offset)
{
    auto rb = make_buffer<T, buffer_size>(offset);
    for (size_t i = 0; i < buffer_size; i++)
    {
        rb.put((T)i);
    }
    TEST_ASSERT_EQUAL(RB_ERROR_ILLEGAL, rb.put((T)buffer_size));
    for (size_t i = 0; i < buffer_size; i++)
    {
        TEST_ASSERT_EQUAL((T)i, rb.get().value);
    }
}

void test_put_to_full(void)
{
    subtest_put_to_full<int32_t, 16>(0);
    subtest_put_to_full<int32_t, 16>(4);
    subtest_put_to_full<int32_t, 16>(8);
    subtest_put_to_full<int32_t, 16>(12);
}

template<typename T, size_t buffer_size>
void subtest_put_array_to_full(size_t offset)
{
    auto rb = make_buffer<T, buffer_size>(offset);
    T data_1[buffer_size] = { 1, 2, 3, 4 };
    T data_2[buffer_size] = { 5, 6 };
    rb.put(data_1, ARRAY_LENGTH(data_1));
    TEST_ASSERT_TRUE(rb.is_full());
    auto result = rb.put(data_2, ARRAY_LENGTH(data_2));
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(0, result.value);
    TEST_ASSERT_EQUAL(1, rb.get().value);
    TEST_ASSERT_EQUAL(2, rb.get().value);
    TEST_ASSERT_EQUAL(3, rb.get().value);
    TEST_ASSERT_EQUAL(4, rb.get().value);
}

void test_put_array_to_full(void)
{
    subtest_put_array_to_full<int32_t, 16>(0);
    subtest_put_array_to_full<int32_t, 16>(4);
    subtest_put_array_to_full<int32_t, 16>(8);
    subtest_put_array_to_full<int32_t, 16>(12);
}

template<typename T, size_t buffer_size>
void subtest_pointer_dance(size_t offset)
{
    auto rb = make_buffer<T, buffer_size>(offset);
    T data_1[buffer_size];

    //  R
    //  W->
    // [...................]

    auto result = rb.put(data_1, buffer_size*3/4);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size*3/4, result.value);

    // R->
    //               W
    // [...................]

    result = rb.get(data_1, buffer_size/2);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/2, result.value);

    //         R->
    //               W
    // [...................]

    result = rb.get(data_1, buffer_size/4);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/4, result.value);

    //               R
    //               W->
    // [...................]

    result = rb.put(data_1, buffer_size/2);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/2, result.value);

    //               R
    //     W->
    // [...................]

    result = rb.put(data_1, buffer_size/4);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/4, result.value);

    //               R->
    //         W
    // [...................]

    result = rb.put(data_1, buffer_size/8);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size/8, result.value);

    //                  R
    //         W->
    // [...................]

    result = rb.put(data_1, buffer_size*1/8);
    TEST_ASSERT_EQUAL(RB_ERROR_NONE, result.error);
    TEST_ASSERT_EQUAL(buffer_size*1/8, result.value);

    //                  R
    //                  W
    // [...................]
}

void test_pointer_dance(void)
{
    subtest_pointer_dance<int32_t, 128>(0);
    subtest_pointer_dance<int32_t, 128>(8);
    subtest_pointer_dance<int32_t, 128>(64);
    subtest_pointer_dance<int32_t, 128>(87);
}

void test_cant_do_anything_while_locked(void)
{
    int32_t arbitrary = -172983;

    auto rb = make_buffer<int32_t, 16>(8);

    global_lock();
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.put(arbitrary));
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.put(&arbitrary, 1).error);
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.get().error);
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.get(&arbitrary, 1).error);
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.is_empty().error);
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.level().error);
    TEST_ASSERT_EQUAL(RB_ERROR_TIMEOUT, rb.available().error);
}

