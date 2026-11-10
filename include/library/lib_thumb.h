#if !defined(DEF_LIB_THUMB_H)
#define DEF_LIB_THUMB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//Thumbnails are plain sidecar files generated on the PC (see README/FTP flow):
//- "<media path>.jpg" (most specific)
//- "<media directory>/poster.jpg"
//- "<media directory>/folder.jpg"

//Returns the media directory ("" when the path has no '/'), as a view inside path.
//The returned length never includes the trailing slash.
size_t Lib_thumb_directory_length(const char* path);

//Builds "<media path>.jpg".
bool Lib_thumb_sidecar_path(const char* media_path, char* out, size_t out_size);

//Builds "<media directory>/poster.jpg" or "folder.jpg".
bool Lib_thumb_fallback_path(const char* media_path, const char* file_name,
char* out, size_t out_size);

//True when name ends with ".jpg"/".jpeg" (case-insensitive).
bool Lib_thumb_is_image(const char* name);

#endif //!defined(DEF_LIB_THUMB_H)
