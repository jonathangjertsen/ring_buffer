This ring buffer implementation uses templates to ensure optimal code can be built for whichever element size and buffer sizes are required by the application.

Most of the complexity is a result of the following 2 features:

1. All public operations are thread-safe.
   This means that most operations may fail if a lock can't be acquired in time.
   Such operations return a rb_result_t structure containing an error code and the actual value.
   The value is only valid if the error code is RB_ERROR_NONE.
2. Large buffer transfers are supported in an efficient way.
   Each transfer is atomic (will only acquire and release the lock once) and only does the
   minimum amount of work.
3. Both overwriting and non-overwriting modes are supported.

Only power-of-two buffer sizes are supported.

Usage examples are provided in `test_ring_buffer.cpp`. These are intended to be built with Unity using its code-generation tool to create an actual test runner.