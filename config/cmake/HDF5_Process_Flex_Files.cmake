# post process flex/bison files

message (STATUS "File: ${FILE_PARSE} ${FILE_ANALYZE}")

if (FILE_PARSE)
  # fix H5LTparse.c to declare H5LTyyparse return type as an hid_t
  # instead of int.  Currently the generated function H5LTyyparse is
  # generated with a return value of type int, which is a mapping to the
  # flex yyparse function.  The return value in the HL library should be
  # an hid_t.
  # I propose to not use flex to generate this function, but for now I am
  # adding a perl command to find and replace this function declaration in
  # H5LTparse.c.
    file (READ ${FILE_PARSE} TEST_STREAM)
    string (REGEX REPLACE "int\nyyparse" "hid_t\nyyparse" TEST_STREAM "${TEST_STREAM}")
    string (REGEX REPLACE "int H5LTyyparse" "hid_t H5LTyyparse" TEST_STREAM "${TEST_STREAM}")
    file (WRITE ${FILE_PARSE} "${TEST_STREAM}")
    message (STATUS "replacing signature in H5LTparse.c")

  # Add code that disables warnings in the flex/bison-generated code.
  #
  # Note that the GCC pragmas did not exist until gcc 4.2. Earlier versions
  # will simply ignore them, but we want to avoid those warnings.
    file (READ ${FILE_PARSE} TEST_STREAM)
    file (WRITE ${FILE_PARSE} "
#if __GNUC__ >= 4 && __GNUC_MINOR__ >=2\n
#pragma GCC diagnostic ignored \"-Wconversion\"\n
#pragma GCC diagnostic ignored \"-Wimplicit-function-declaration\"\n
#pragma GCC diagnostic ignored \"-Wlarger-than=\"\n
#pragma GCC diagnostic ignored \"-Wmissing-prototypes\"\n
#pragma GCC diagnostic ignored \"-Wnested-externs\"\n
#pragma GCC diagnostic ignored \"-Wold-style-definition\"\n
#pragma GCC diagnostic ignored \"-Wsign-compare\"\n
#pragma GCC diagnostic ignored \"-Wsign-conversion\"\n
#pragma GCC diagnostic ignored \"-Wstrict-prototypes\"\n
#pragma GCC diagnostic ignored \"-Wswitch-default\"\n
#pragma GCC diagnostic ignored \"-Wunused-function\"\n
#pragma GCC diagnostic ignored \"-Wunused-macros\"\n
#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n
#pragma GCC diagnostic ignored \"-Wredundant-decls\"\n
#elif defined __SUNPRO_CC\n
#pragma disable_warn\n
#elif defined _MSC_VER\n
#pragma warning(push, 1)\n
#endif\n
    ")
    file (APPEND ${FILE_PARSE} "${TEST_STREAM}")
    message (STATUS "processing pragma in ${FILE_PARSE}")
    EXECUTE_PROCESS (
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${FILE_PARSE}.timestamp
    )
endif (FILE_PARSE)

if (FILE_ANALYZE)
  # Add code that disables warnings in the flex/bison-generated code.
  #
  # Note that the GCC pragmas did not exist until gcc 4.2. Earlier versions
  # will simply ignore them, but we want to avoid those warnings.
    file (READ ${FILE_ANALYZE} TEST_STREAM)
    file (WRITE ${FILE_ANALYZE} "
#if __GNUC__ >= 4 && __GNUC_MINOR__ >=2\n
#pragma GCC diagnostic ignored \"-Wconversion\"\n
#pragma GCC diagnostic ignored \"-Wimplicit-function-declaration\"\n
#pragma GCC diagnostic ignored \"-Wlarger-than=\"\n
#pragma GCC diagnostic ignored \"-Wmissing-prototypes\"\n
#pragma GCC diagnostic ignored \"-Wnested-externs\"\n
#pragma GCC diagnostic ignored \"-Wold-style-definition\"\n
#pragma GCC diagnostic ignored \"-Wsign-compare\"\n
#pragma GCC diagnostic ignored \"-Wsign-conversion\"\n
#pragma GCC diagnostic ignored \"-Wstrict-prototypes\"\n
#pragma GCC diagnostic ignored \"-Wswitch-default\"\n
#pragma GCC diagnostic ignored \"-Wunused-function\"\n
#pragma GCC diagnostic ignored \"-Wunused-macros\"\n
#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n
#pragma GCC diagnostic ignored \"-Wredundant-decls\"\n
#elif defined __SUNPRO_CC\n
#pragma disable_warn\n
#elif defined _MSC_VER\n
#pragma warning(push, 1)\n
#endif\n
    ")
    file (APPEND ${FILE_ANALYZE} "${TEST_STREAM}")
    message (STATUS "processing pragma in ${FILE_ANALYZE}")
    EXECUTE_PROCESS (
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${FILE_ANALYZE}.timestamp
    )
endif (FILE_ANALYZE)