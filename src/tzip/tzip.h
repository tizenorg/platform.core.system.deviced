/*
 * deviced
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TZIP_H__
#define __TZIP_H__


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief mounts a zip file to a particular system path.
 * @remarks This is asynchronous method, zip file is mounted or not should be checked using tzip_is_mounted(mount_path) before using mounted path.
 * @since_tizen 3.0
 * @param[in] zip_file The input zip file
 * @param[in] mount_path The mount path
 * @return @c 0 if the mount operation is successful, \n @c negative error code if the mount operation fails
 */
int tzip_mount_zipfs(const char *zip_file, const char *mount_path);

/**
 * @brief unmounts a already mounted path
 * @since_tizen 3.0
 * @param[in] mount_path The mounted path
 * @return @c 0 if the unmount operation is successful, \n @c negative error code if the unmount operation fails
 */
int tzip_unmount_zipfs(const char *mount_path);

/**
 * @brief checks if given path is mounted
 * @since_tizen 3.0
 * @param[in] mount_path The mount path
 * @return @c 1 if the given path is mounted and 0 if it is not mounted, \n @c negative error code if this operation fails
 */
int tzip_is_mounted(const char *mount_path);

#ifdef __cplusplus
}
#endif

#endif // __TZIP_H__
