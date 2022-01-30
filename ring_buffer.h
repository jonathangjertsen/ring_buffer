/**
 * @file      ring_buffer.h
 * @brief     Generic thread-safe ring buffer
 * @author    Jonathan Reichelt Gjertsen
 *
 * See README.md
 */

#ifndef RB_H
#define RB_H

#include <array>
#include <stddef.h>
#include <string.h>

/**
 * Error code associated with actions that must acquire a lock before proceeding.
 */
typedef enum
{
    RB_ERROR_NONE    = 0,   /**< The operation was completed successfully */
    RB_ERROR_ILLEGAL = 1,   /**< The operation was illegal */
    RB_ERROR_TIMEOUT = 2,   /**< Timed out before the lock could be acquired */
    RB_ERROR_OVERWRITE = 3, /**< The operation resulted in overwriting unread data */
} rb_error_t;

/**
 * Result type associated with actions that must acquire a lock before proceeding.
 *
 * @tparam T The type of the value
 */
template<class T>
struct rb_result_t
{
    rb_error_t error; /**< The error code of the operation */
    T value;          /**< The return value of the operation. Only valid if error == RB_ERROR_NONE. */
};

/**
 * Ring buffer
 *
 * @tparam T               The type of each element in the ring buffer
 * @tparam buffer_size     The number of elements in the buffer
 * @tparam allow_overwrite The number of elements in the buffer
 * @tparam lock            A function which attempts to acquire a lock and returns whether the operation succeeded
 * @tparam unlock          A function which releases the same lock that is acquired by the lock function
 */
template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
class RingBuffer
{
    /**
     * Statically assert that only buffer sizes that are powers of two are used
     *
     * http://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
     */
    static_assert(buffer_size && !(buffer_size & (buffer_size - 1)), "buffer_size must be a power of two");
public:
    explicit RingBuffer() = default;

    /**
     * Put an item on the ring buffer
     *
     * @param item The item to put
     *
     * @retval RB_ERROR_NONE    Item was successfully put on the queue
     * @retval RB_ERROR_ILLEGAL Item could not be put on the queue because it was full
     * @retval RB_ERROR_TIMEOUT Timed out before the lock could be acquired
     */
    rb_error_t put(const T &item);

    /**
     * Put many items on the ring buffer, performing at most 2 copy operations
     *
     * @param items   Start of memory for items to be put on the queue
     * @param n_items Number of items to put on the queue
     *
     * @return The return value has two fields: an error and a value.
     *
     * The value is only valid if the error is RB_ERROR_NONE.
     * In this case, result.value is the number of items that were actually put on the queue.
     *
     * If `auto result = rb.get()`, then:
     * - `result.error == RB_ERROR_NONE`:    Item was successfully retrieved from the queue. result.value is valid.
     * - `result.error == RB_ERROR_TIMEOUT`: Timed out before the lock could be acquired. result.value is invalid.
     */
    rb_result_t<size_t> put(const T *items, size_t n_items);

    /**
     * Get an item from the ring buffer
     *
     * The return value has two fields: an error and a value.
     * The value is only valid if the error is RB_ERROR_NONE.
     *
     * If `auto result = rb.get()`, then:
     * - `result.error == RB_ERROR_NONE`:    Item was successfully retrieved from the queu. result.value is valid.
     * - `result.error == RB_ERROR_ILLEGAL`: The queue was empty. result.value is invalid.
     * - `result.error == RB_ERROR_TIMEOUT`: Timed out before the lock could be acquired. result.value is invalid.
     */
    rb_result_t<T> get();

    /**
     * Get many items from the ring buffer, performing at most 2 copy operations
     *
     * @param[out] items   Start of memory for items to be retrieved from the queue
     * @param      n_items Number of items to get from the queue
     *
     * @return The return value has two fields: an error and a value.
     *
     * The value is only valid if the error is RB_ERROR_NONE.
     * In this case, result.value is the number of items that were actually retrieved from the queue.
     *
     * If `auto result = rb.get()`, then:
     * - `result.error == RB_ERROR_NONE`:    Item was successfully retrieved from the queue. result.value is valid.
     * - `result.error == RB_ERROR_TIMEOUT`: Timed out before the lock could be acquired. result.value is invalid.
     */
    rb_result_t<size_t> get(T *items, size_t n_items);

    /**
     * Reset the ring buffer
     *
     * @retval RB_ERROR_NONE    Ring buffer was reset
     * @retval RB_ERROR_TIMEOUT Timed out before the lock could be acquired
     */
    rb_error_t reset();

    /**
     * Query whether the queue is empty
     *
     * The return value has two fields: an error and a value.
     * The value is only valid if the error is RB_ERROR_NONE.
     *
     * If `auto result = rb.is_empty()`, then:
     * - `result.error == RB_ERROR_NONE`:    result.value contains whether the queue was empty.
     * - `result.error == RB_ERROR_TIMEOUT`: Timed out before the lock could be acquired. result.value is invalid.
     */
    rb_result_t<bool> is_empty();

    /**
     * Returns whether the ring buffer is full.
     */
    bool is_full() { return full; }

    /**
     * Query the level of the buffer.
     *
     * The return value has two fields: an error and a value.
     * The value is only valid if the error is RB_ERROR_NONE.
     *
     * If `auto result = rb.is_empty()`, then:
     * - `result.error == RB_ERROR_NONE`:    result.value contains the level of the buffer.
     * - `result.error == RB_ERROR_TIMEOUT`: Timed out before the lock could be acquired. result.value is invalid.
     */
    rb_result_t<size_t> level();

    /**
     * Query the remaining space available in the buffer.
     *
     * The return value has two fields: an error and a value.
     * The value is only valid if the error is RB_ERROR_NONE.
     *
     * If `auto result = rb.is_empty()`, then:
     * - `result.error == RB_ERROR_NONE`:    result.value contains the remaining space available in the buffer.
     * - `result.error == RB_ERROR_TIMEOUT`: Timed out before the lock could be acquired. result.value is invalid.
     */
    rb_result_t<size_t> available();

    /**
     * Advance the internal pointers for testing with wraparound
     */
    void testonly_advance_pointers(size_t n)
    {
        T dummy;
        for (size_t i = 0; i < n; i++)
        {
            put(dummy);
        }
        for (size_t i = 0; i < n; i++)
        {
            get();
        }
    }
private:
    T buffer[buffer_size];
    size_t write_index = 0;
    size_t read_index = 0;
    bool full = 0;

    size_t unsafe_level()
    {
        size_t result;
        if (full)
        {
            result = buffer_size;
        }
        else if (write_index >= read_index)
        {
            result = write_index - read_index;
        }
        else
        {
            result = buffer_size + write_index - read_index;
        }
        return result;
    }

    bool unsafe_is_empty()
    {
        return (write_index == read_index) && !full;
    }

    void unsafe_mark_full(void)
    {
        write_index = read_index;
        full = true;
    }

    void unsafe_mark_empty(void)
    {
        read_index = write_index;
        full = false;
    }
};

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_error_t RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::put(const T &item)
{
    rb_error_t result = RB_ERROR_NONE;

    if (lock())
    {
        bool do_write = true;
        if (full)
        {
            if (allow_overwrite)
            {
                // Allow overwrite; this means the read index must be eupdated as well
                result = RB_ERROR_OVERWRITE;
                read_index++;
                read_index &= (buffer_size - 1);
                do_write = true;
            }
            else
            {
                // Failed to enqueue the item since the buffer was full.
                result = RB_ERROR_ILLEGAL;
                do_write = false;
            }
        }
        else
        {
            result = RB_ERROR_NONE;
            do_write = true;
        }

        if (do_write)
        {
            // Buffer is not full. Write the item.
            buffer[write_index] = item;

            // Increment write_index, rolling over if required
            write_index++;
            write_index &= (buffer_size - 1);

            // Note down whether we are now full
            full = write_index == read_index;
        }

        unlock();
    }
    else
    {
        // Failed to enqueue the item since the lock could not be acquired.
        result = RB_ERROR_TIMEOUT;
    }

    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_result_t<size_t> RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::put(const T *items, size_t n_items)
{
    rb_result_t<size_t> result = {
        .error = RB_ERROR_NONE,
        .value = 0
    };

    if (lock())
    {
        bool do_write = true;
        if (full)
        {
            if (allow_overwrite)
            {
                // Allow overwrite; this means the read index must be updated as well
                result.error = RB_ERROR_OVERWRITE;
                read_index += std::min(n_items, buffer_size);
                read_index &= (buffer_size - 1);
                do_write = true;
            }
            else
            {
                // Failed to enqueue the item since the buffer was full.
                result.value = 0;
                do_write = false;
            }
        }
        else
        {
            result.error = RB_ERROR_NONE;
            do_write = true;
        }

        if (do_write)
        {
            if (write_index >= read_index)
            {
                // Write index is past the read index, so we may need to deal with wrapping around
                size_t available_until_end_of_buffer = buffer_size - write_index;
                if (n_items >= available_until_end_of_buffer)
                {
                    // First, write up to the end of the buffer
                    memcpy(&buffer[write_index], items, available_until_end_of_buffer * sizeof(T));

                    // Then, start writing up to the read pointer
                    size_t n_items_remaining = n_items - available_until_end_of_buffer;
                    if (n_items_remaining < read_index)
                    {
                        // We can write all remaining items without filling the buffer
                        memcpy(buffer, &items[available_until_end_of_buffer], n_items_remaining * sizeof(T));
                        write_index = n_items_remaining;
                        result.value = n_items;
                    }
                    else
                    {
                        // We will fill the buffer before we complete
                        memcpy(buffer, &items[available_until_end_of_buffer], read_index * sizeof(T));
                        unsafe_mark_full();
                        result.value = available_until_end_of_buffer + read_index;
                    }
                }
                else
                {
                    // We can write all the items without wrapping around
                    memcpy(&buffer[write_index], items, n_items * sizeof(T));
                    write_index += n_items;
                    result.value = n_items;
                }
            }
            else
            {
                // Write index is behind the read index, so all available slots are contiguous in memory
                size_t available = read_index - write_index;
                if (n_items < available)
                {
                    // We can write all items without filling the buffer
                    memcpy(&buffer[write_index], items, n_items * sizeof(T));
                    write_index += n_items;
                    result.value = n_items;
                }
                else
                {
                    // We will fill the buffer before we complete
                    memcpy(&buffer[write_index], items, available * sizeof(T));
                    unsafe_mark_full();
                    result.value = available;
                }
            }
        }
        else
        {
            // No write
        }

        unlock();
    }
    else
    {
        // Failed to enqueue the items since the lock could not be acquired.
        result.error = RB_ERROR_TIMEOUT;
    }

    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_result_t<T> RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::get()
{
    rb_result_t<T> result = {
        .error = RB_ERROR_NONE,
        .value = T()
    };

    if (lock())
    {
        if (unsafe_is_empty())
        {
            // Failed to get an item since the queue was empty.
            result.error = RB_ERROR_ILLEGAL;
        }
        else
        {
            // Read the item off the buffer
            result.value = buffer[read_index];

            // Increment the read_index, wrapping if required
            read_index++;
            read_index &= (buffer_size - 1);

            // We are guaranteed to have at least one element
            full = false;
        }

        unlock();
    }
    else
    {
        // Failed to get an item since the lock could not be acquired.
        result.error = RB_ERROR_TIMEOUT;
    }

    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_result_t<size_t> RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::get(T *items, size_t n_items)
{
    rb_result_t<size_t> result = {
        .error = RB_ERROR_NONE,
        .value = 0
    };

    if (lock())
    {
        if (unsafe_is_empty())
        {
            // Failed to retrieve any items since the buffer was empty.
            result.value = 0;
        }
        else if (write_index > read_index)
        {
            // Write index is past the read index, so all available items are contiguous in memory
            size_t available = write_index - read_index;
            if (n_items < available)
            {
                // We can read all items without emptying the buffer
                memcpy(items, &buffer[read_index], n_items * sizeof(T));
                read_index += n_items;
                result.value = n_items;
            }
            else
            {
                // We will empty the buffer before we complete
                memcpy(items, &buffer[read_index], available * sizeof(T));
                unsafe_mark_empty();
                result.value = available;
            }
        }
        else
        {
            // Write index is behind the read index, so we may need to deal with wrapping around
            size_t available_until_end_of_buffer = buffer_size - read_index;
            if (n_items >= available_until_end_of_buffer)
            {
                // First, read up to the end of the buffer
                memcpy(items, &buffer[read_index], available_until_end_of_buffer * sizeof(T));

                // Then, start reading up to the write pointer
                size_t n_items_remaining = n_items - available_until_end_of_buffer;
                if (n_items_remaining < write_index)
                {
                    // We can read all remaining items without emptying the buffer
                    memcpy(&items[available_until_end_of_buffer], buffer, n_items_remaining * sizeof(T));
                    read_index = n_items_remaining;
                    result.value = n_items;
                }
                else
                {
                    // We will empty the buffer before we complete
                    memcpy(&items[available_until_end_of_buffer], buffer, write_index * sizeof(T));
                    unsafe_mark_empty();
                    result.value = available_until_end_of_buffer + write_index;
                }
            }
            else
            {
                // We can read all the items without wrapping around
                memcpy(items, &buffer[write_index], n_items * sizeof(T));
                read_index += n_items;
                result.value = n_items;
            }
        }

        unlock();
    }
    else
    {
        // Failed to enqueue the item since the lock could not be acquired.
        result.error = RB_ERROR_TIMEOUT;
    }

    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_error_t RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::reset()
{
    rb_error_t result = RB_ERROR_NONE;
    if (lock())
    {
        write_index = read_index;
        full = false;

        unlock();
    }
    else
    {
        result = RB_ERROR_TIMEOUT;
    }
    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_result_t<bool> RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::is_empty()
{
    rb_result_t<bool> result = {
        .error = RB_ERROR_ILLEGAL,
        .value = true
    };

    if (lock())
    {
        result.value = unsafe_is_empty();
        result.error = RB_ERROR_NONE;
        unlock();
    }
    else
    {
        result.error = RB_ERROR_TIMEOUT;
    }

    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_result_t<size_t> RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::level()
{
    rb_result_t<size_t> result = {
        .error = RB_ERROR_NONE,
        .value = 0U
    };

    if (lock())
    {
        result.value = unsafe_level();
        unlock();
    }
    else
    {
        result.error = RB_ERROR_TIMEOUT;
    }
    return result;
}

template<class T, size_t buffer_size, bool allow_overwrite, bool (*lock)(void), void (*unlock)(void)>
rb_result_t<size_t> RingBuffer<T, buffer_size, allow_overwrite, lock, unlock>::available()
{
    rb_result_t<size_t> result = {
        .error = RB_ERROR_NONE,
        .value = 0U
    };

    if (lock())
    {
        result.value = buffer_size - unsafe_level();
        unlock();
    }
    else
    {
        result.error = RB_ERROR_TIMEOUT;
    }
    return result;
}

#endif
