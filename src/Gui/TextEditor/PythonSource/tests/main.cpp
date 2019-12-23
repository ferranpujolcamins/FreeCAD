
#include <gtest/gtest.h>
#include <Python.h>

int main(int argc, char *argv[])
{
    // init python
    wchar_t *program = Py_DecodeLocale(argv[0], nullptr);
    if (program == nullptr) {
        fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
        exit(1);
    }
    /* Pass argv[0] to the Python interpreter */
    Py_SetProgramName(program);
    /* Initialize the Python interpreter.  Required. */
    Py_Initialize();
    PyMem_RawFree(program);


    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
