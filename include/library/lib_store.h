#if !defined(DEF_LIB_STORE_H)
#define DEF_LIB_STORE_H
#include <stdbool.h>
#include "library/lib_index.h"

//Creates the library directories ("library/" and "library/thumbs/").
bool Lib_store_ensure_dirs(void);

//Loads "library/index.bin", falling back to "library/index.bak".
//Returns LIB_INDEX_ERR_IO when there is no readable index (rebuild it).
Lib_index_result Lib_store_load(Lib_index* index);

//Writes "library/index.bin" through "index.tmp" and keeps the previous file
//as "index.bak" for recovery.
Lib_index_result Lib_store_save(const Lib_index* index);

#endif //!defined(DEF_LIB_STORE_H)
