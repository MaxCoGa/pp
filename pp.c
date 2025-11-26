#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>

#define UPDATE_FLAG 0
#define SECURITY_UPDATE_FLAG 1
#define MANDATORY_PKG_FLAG 2
#define OPTIONAL_PKG_FLAG 3
#define REMOVED_PKG_FLAG 4
#define MANUAL_PKG_FLAG 5

// Package info
typedef struct {
    char name[50];
    char version[20];
    char sha256[65];
    char url[256];
    int package_status; // 0=update, 1=security update, 2=mandatory, 3=optional, 4=removed, 5=manual
    int present_in_repository; // 0=not present, 1=exist
} Package;

Package *local_packages = NULL;
int local_package_count = 0;
int allocated_packages = 0;

// find a package in the local_packages array by exact name
int find_local_package(const char *package_name) {
    for (int i = 0; i < local_package_count; i++) {
        if (strcmp(local_packages[i].name, package_name) == 0) {
            return i;
        }
    }
    return -1;
}

// write the local_packages array to pp_pkg_list (removed packages will have the REMOVED_PKG_FLAG flag in pp_pkg_list)
void write_local_package_list() {
    FILE *file = fopen("pp_pkg_list", "w");
    if (file == NULL) {
        perror("Error opening pp_pkg_list for writing");
        return;
    }

    for (int i = 0; i < local_package_count; i++) {
        fprintf(file, "%s %s %s %s %d\n",
                local_packages[i].name,
                local_packages[i].version,
                local_packages[i].sha256,
                local_packages[i].url,
                local_packages[i].package_status);
    }
    fclose(file);
}

// read and parse pkg_list (repository source) and update local_packages TODO: update local_packages in another function
void read_repository_package_list() {
    FILE *file = fopen("pkg_list", "r"); // TODO: should be a url
    if (file == NULL) {
        perror("Error opening pkg_list");
        return;
    }

    char line[256]; // TODO

    int repository_package_count = 0;
    Package *repository_packages = NULL;
    int allocated_repository_packages = 0;

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *package_name = strtok(line_copy, " ");
        char *version = strtok(NULL, " ");
        char *sha256 = strtok(NULL, " ");
        char *url = strtok(NULL, " ");
        char *package_status_str = strtok(NULL, " ");

        if (package_name && version && sha256 && url && package_status_str) {

             // dynamic allocation
            if (repository_package_count >= allocated_repository_packages) {
                int new_size = (allocated_repository_packages == 0) ? 10 : allocated_repository_packages * 2; // start at 10, double
                Package *temp = realloc(repository_packages, new_size * sizeof(Package));
                if (temp == NULL) {
                    perror("Error reallocating memory for repository packages");
                    fclose(file);
                    if (repository_packages != NULL) {
                        free(repository_packages);
                    }
                    return; // exit(1); ?
                }
                repository_packages = temp;
                allocated_repository_packages = new_size;
            }

            strncpy(repository_packages[repository_package_count].name, package_name, sizeof(repository_packages[0].name) - 1);
            repository_packages[repository_package_count].name[sizeof(repository_packages[0].name) - 1] = '\0';
            strncpy(repository_packages[repository_package_count].version, version, sizeof(repository_packages[0].version) - 1);
            repository_packages[repository_package_count].version[sizeof(repository_packages[0].version) - 1] = '\0';
            strncpy(repository_packages[repository_package_count].sha256, sha256, sizeof(repository_packages[0].sha256) - 1);
            repository_packages[repository_package_count].sha256[sizeof(repository_packages[0].sha256) - 1] = '\0';
            strncpy(repository_packages[repository_package_count].url, url, sizeof(repository_packages[0].url) - 1);
            repository_packages[repository_package_count].url[sizeof(repository_packages[0].url) - 1] = '\0';
            repository_packages[repository_package_count].package_status = atoi(package_status_str);
            repository_package_count++;
        } else {
            printf("Skipping invalid line in pkg_list: %s\n", line);
        }
    }
    fclose(file);
    printf("Read %d packages from pkg_list.\n", repository_package_count); // remote repo

    // compare remote repository packages with local packages and update local_packages present flag
    for (int i = 0; i < local_package_count; i++) {
        local_packages[i].present_in_repository = 0; // set all to not found
    }

    for (int i = 0; i < repository_package_count; i++) {
        int index = find_local_package(repository_packages[i].name);

        if (index != -1) {
            local_packages[index].present_in_repository = 1;

            // check for updates in version, sha256, url, or status(status outside for now to handle more use cases)
            if (strcmp(local_packages[index].version, repository_packages[i].version) != 0 ||
                strcmp(local_packages[index].sha256, repository_packages[i].sha256) != 0 ||
                strcmp(local_packages[index].url, repository_packages[i].url) != 0) {

                printf("Updating package %s: Version %s -> %s, sha256 %s -> %s, URL %s -> %s\n",
                       repository_packages[i].name,
                       local_packages[index].version, repository_packages[i].version,
                       local_packages[index].sha256, repository_packages[i].sha256,
                       local_packages[index].url, repository_packages[i].url);

                strncpy(local_packages[index].version, repository_packages[i].version, sizeof(local_packages[index].version) - 1);
                local_packages[index].version[sizeof(local_packages[index].version) - 1] = '\0';
                strncpy(local_packages[index].sha256, repository_packages[i].sha256, sizeof(local_packages[index].sha256) - 1);
                local_packages[index].sha256[sizeof(local_packages[index].sha256) - 1] = '\0';
                strncpy(local_packages[index].url, repository_packages[i].url, sizeof(local_packages[index].url) - 1);
                local_packages[index].url[sizeof(local_packages[index].url) - 1] = '\0';

            } else {
                printf("Package %s is up to date.\n", repository_packages[i].name);
            }

            if (local_packages[index].package_status != repository_packages[i].package_status) {
                 printf("Updating package status for %s: %d -> %d\n",
                        repository_packages[i].name, local_packages[index].package_status, 
                        repository_packages[i].package_status);
                local_packages[index].package_status = repository_packages[i].package_status;
            }
        } else { // package from repository not found in local list
            // dynamic allocation
            if (local_package_count >= allocated_packages) {
                int new_size = (allocated_packages == 0) ? 10 : allocated_packages * 2; // start at 10, double
                Package *temp = realloc(local_packages, new_size * sizeof(Package));
                if (temp == NULL) {
                    perror("Error reallocating memory for local packages while adding from repository");
                    if (repository_packages != NULL) free(repository_packages);
                    return; // exit(1); ?
                }
                local_packages = temp;
                allocated_packages = new_size;
                // printf("DEBUG: Reallocated local_packages to size %d\n", allocated_packages);
            }

            printf("Adding new package to local list from repository: %s\n", repository_packages[i].name);
            strncpy(local_packages[local_package_count].name, repository_packages[i].name, sizeof(local_packages[0].name) - 1);
            local_packages[local_package_count].name[sizeof(local_packages[0].name) - 1] = '\0';
            strncpy(local_packages[local_package_count].version, repository_packages[i].version, sizeof(local_packages[0].version) - 1);
            local_packages[local_package_count].version[sizeof(local_packages[0].version) - 1] = '\0';
            strncpy(local_packages[local_package_count].sha256, repository_packages[i].sha256, sizeof(local_packages[0].sha256) - 1);
            local_packages[local_package_count].sha256[sizeof(local_packages[0].sha256) - 1] = '\0';
            strncpy(local_packages[local_package_count].url, repository_packages[i].url, sizeof(local_packages[0].url) - 1);
            local_packages[local_package_count].url[sizeof(local_packages[0].url) - 1] = '\0';
            local_packages[local_package_count].package_status = repository_packages[i].package_status; 
            local_packages[local_package_count].present_in_repository = 1;
            local_package_count++;
        }
    }

    // free memory allocated for repository_packages
    if (repository_packages != NULL) {
        free(repository_packages);
        repository_packages = NULL;
    }

    // FLAG packages in local list that are no longer in the remote repository
    for (int i = 0; i < local_package_count; i++) {
        if (!local_packages[i].present_in_repository) {
            if (local_packages[i].package_status != 4) {
                local_packages[i].package_status = 4;
                printf("Package %s no longer exists in the repository.\n", local_packages[i].name);
            }
        }
    }

     if (allocated_packages > local_package_count * 2) { // If allocated is more than double needed
         int new_size = (local_package_count == 0) ? 0 : local_package_count * 2;
         Package *temp = realloc(local_packages, new_size * sizeof(Package));
         if (temp != NULL || new_size == 0) { // realloc can return NULL for size 0, which is valid for freeing
              local_packages = temp;
             allocated_packages = new_size;
            //  printf("DEBUG: Shrunk local_packages to size %d\n", allocated_packages);
         }
     }
}

// read and parse pp_pkg_list(local package list)
void read_local_package_list() {
    FILE *file = fopen("pp_pkg_list", "r");
    if (file == NULL) {
        // local_packages to NULL and allocated_packages to 0
        if (local_packages != NULL) {
            free(local_packages);
            local_packages = NULL;
        }
        local_package_count = 0;
        allocated_packages = 0;
        printf("pp_pkg_list not found. Initializing empty local package list.\n");
        return;
    }

    // local_packages already has memory allocated, free it before re-reading
    if (local_packages != NULL) {
        free(local_packages);
        local_packages = NULL;
    }
    local_package_count = 0;
    allocated_packages = 0;

    char line[256]; // TODO: can be longer
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        char line_copy[256];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *package_name = strtok(line_copy, " ");
        char *version = strtok(NULL, " ");
        char *sha256 = strtok(NULL, " ");
        char *url = strtok(NULL, " ");
        char *package_status_str = strtok(NULL, " ");

        if (package_name && version && sha256 && url && package_status_str) { 
             int package_status = atoi(package_status_str); // Convert status string to integer

            // dynamic allocation
            if (local_package_count >= allocated_packages) {
                int new_size = (allocated_packages == 0) ? 10 : allocated_packages * 2;
                Package *temp = realloc(local_packages, new_size * sizeof(Package));
                if (temp == NULL) {
                    perror("Error reallocating memory for packages while reading pp_pkg_list");
                     fclose(file);
                     return; // exit(1); free(local_packages);?
                }
                local_packages = temp;
                allocated_packages = new_size;
                // printf("DEBUG: Reallocated local_packages to size %d\n", allocated_packages);
            }

            strncpy(local_packages[local_package_count].name, package_name, sizeof(local_packages[0].name) - 1);
            local_packages[local_package_count].name[sizeof(local_packages[0].name) - 1] = '\0';
            strncpy(local_packages[local_package_count].version, version, sizeof(local_packages[0].version) - 1);
            local_packages[local_package_count].version[sizeof(local_packages[0].version) - 1] = '\0';
            strncpy(local_packages[local_package_count].sha256, sha256, sizeof(local_packages[0].sha256) - 1);
            local_packages[local_package_count].sha256[sizeof(local_packages[0].sha256) - 1] = '\0';
            strncpy(local_packages[local_package_count].url, url, sizeof(local_packages[0].url) - 1);
            local_packages[local_package_count].url[sizeof(local_packages[0].url) - 1] = '\0';
            local_packages[local_package_count].package_status = package_status;
            local_packages[local_package_count].present_in_repository = 0;

            local_package_count++;
        } else {
            printf("Skipping invalid line in pp_pkg_list: %s\n", line);
        }
    }

    fclose(file);
    printf("Read %d packages from pp_pkg_list.\n", local_package_count);
}

// search for a package
void search_package(const char *search_term) {
    printf("Searching for packages matching: %s\n", search_term);

    int found_count = 0;
    for (int i = 0; i < local_package_count; i++) {
        if (strstr(local_packages[i].name, search_term) != NULL) {
            printf("Found package: Name=%s, Version=%s, sha256=%s, URL=%s\n",
                   local_packages[i].name,
                   local_packages[i].version,
                   local_packages[i].sha256,
                   local_packages[i].url);
            found_count++;
        }
    }

    if (found_count == 0) {
        printf("No packages found matching '%s'.\n", search_term);
    }
}

// find and print information for a package by exact name
void exact_search_package(const char *package_name_to_find) {
    printf("Searching for exact package match: %s\n", package_name_to_find);

    int found_index = find_local_package(package_name_to_find);

    if (found_index != -1) {
        printf("Found package: Name=%s, Version=%s, sha256=%s, URL=%s\n",
               local_packages[found_index].name,
               local_packages[found_index].version,
               local_packages[found_index].sha256,
               local_packages[found_index].url);
    } else {
        printf("Package '%s' not found in local package list.\n", package_name_to_find);
    }
}

// install a package
void install_package(const char *package_name) {
    printf("Attempting to install package: %s\n", package_name);

    read_local_package_list(); // local package list is loaded
    int package_index = find_local_package(package_name);

    if (package_index == -1) {
        printf("Error: Package '%s' not found in local package list. Cannot install.\n", package_name);
        return;
    }

    const char *package_url = local_packages[package_index].url;
    const char *package_sha256 = local_packages[package_index].sha256;
    printf("Package URL: %s\n", package_url);

    char confirm_install[10];
    printf("Install %s? (Y/n): ", package_name);
    if (fgets(confirm_install, sizeof(confirm_install), stdin) != NULL) {
        confirm_install[strcspn(confirm_install, "\n")] = 0;

        if (strlen(confirm_install) == 0 ||
            strcmp(confirm_install, "Y") == 0 || strcmp(confirm_install, "y") == 0) {

            printf("Installing %s...\n", package_name);

            printf("Creating pp_download directory...\n");
            if (mkdir("pp_download", 0755) == -1) {
                if (errno != EEXIST) {
                    perror("Error creating pp_download directory");
                    return;
                }
            }

            // download the package file to pp_download
            char download_path[512];
            char url_copy[256]; 
            strncpy(url_copy, package_url, sizeof(url_copy) - 1);
            url_copy[sizeof(url_copy) - 1] = '\0';
            char *filename = basename(url_copy);

            snprintf(download_path, sizeof(download_path), "pp_download/%s", filename);
            printf("Destination path: %s\n", download_path);


            if (strncmp(package_url, "http://", 7) == 0 || strncmp(package_url, "https://", 8) == 0) { // url
                printf("Downloading package from %s...\n", package_url);
                char curl_command[1024];
                snprintf(curl_command, sizeof(curl_command), "curl -L \"%s\" -o \"%s\"", package_url, download_path);
                printf("Executing command: %s\n", curl_command);
                int curl_status = system(curl_command);
                if (curl_status != 0) {
                    printf("Error downloading package: curl command failed with status %d\n", curl_status);
                    return;
                }
                 printf("Download complete.\n");
            } else { // local file path
                printf("Copying package from local path %s...\n", package_url);
                FILE *source_file = fopen(package_url, "rb");
                if (source_file == NULL) {
                    perror("Error opening local package file");
                    return;
                }

                FILE *dest_file = fopen(download_path, "wb");
                if (dest_file == NULL) {
                    perror("Error creating destination file in pp_download");
                    fclose(source_file);
                    return;
                }

                char buffer[4096];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
                    fwrite(buffer, 1, bytes_read, dest_file);
                }

                fclose(source_file);
                fclose(dest_file);
                printf("Copy complete.\n");
            }

            // untar the file into pp_download
            char untar_dir[512];
            snprintf(untar_dir, sizeof(untar_dir), "pp_download/%s", package_name);
            printf("Creating untar directory: %s\n", untar_dir);
             if (mkdir(untar_dir, 0755) == -1) {
                if (errno != EEXIST) { // directory already exist
                    perror("Error creating untar directory");
                    return;
                }
            }

            char tar_command[1024];
            snprintf(tar_command, sizeof(tar_command), "tar -xf \"%s\" -C \"%s\"", download_path, untar_dir);
            printf("Executing command: %s\n", tar_command);
            int tar_status = system(tar_command);
             if (tar_status != 0) {
                printf("Error untarring package: tar command failed with status %d\n", tar_status);
                return;
            }
            printf("Untar complete.\n");

            // find and read the MANIFEST
            char manifest_path[512];
            snprintf(manifest_path, sizeof(manifest_path), "%s/MANIFEST", untar_dir);
            printf("Looking for MANIFEST file at: %s\n", manifest_path);

            FILE *manifest_file = fopen(manifest_path, "r");
            char full_manifest_content[4096] = ""; // Assuming MANIFEST is not larger than 4KB
            if (manifest_file == NULL) {
                perror("Error opening MANIFEST file");
                printf("MANIFEST file not found at %s. Skipping manifest-related steps.\n", manifest_path);
            } else {
                printf("--- MANIFEST ---\n");
                char manifest_line[256];
                while (fgets(manifest_line, sizeof(manifest_line), manifest_file)) {
                    printf("%s", manifest_line);
                    strncat(full_manifest_content, manifest_line, sizeof(full_manifest_content) - strlen(full_manifest_content) - 1);
                }
                printf("----------------\n");
                fclose(manifest_file);

                // save MANIFEST and uninstall script to pp_info/PACKAGENAME/. -> fake database
                char pp_info_dir[512];
                snprintf(pp_info_dir, sizeof(pp_info_dir), "pp_info/%s", package_name);
                printf("Creating package info directory: %s\n", pp_info_dir);
                if (mkdir("pp_info", 0755) == -1) {
                     if (errno != EEXIST) {
                        perror("Error creating pp_info directory");
                        return; // todo?
                    }
                }
                if (mkdir(pp_info_dir, 0755) == -1) {
                     if (errno != EEXIST) {
                        perror("Error creating package info directory");
                        return;
                    }
                } else {
                     // save MANIFEST
                    char saved_manifest_path[512];
                    snprintf(saved_manifest_path, sizeof(saved_manifest_path), "%s/MANIFEST", pp_info_dir);
                    printf("Saving MANIFEST to: %s\n", saved_manifest_path);
                    FILE *saved_manifest_file = fopen(saved_manifest_path, "w");
                    if (saved_manifest_file == NULL) {
                        perror("Error saving MANIFEST file");
                    } else {
                        fprintf(saved_manifest_file, "%s", full_manifest_content);
                        fclose(saved_manifest_file);
                    }

                    // parse MANIFEST
                    char *uninstall_script_line = strstr(full_manifest_content, "uninstall:");
                    if (uninstall_script_line != NULL) {
                        char *uninstall_script_name = uninstall_script_line + strlen("uninstall:");
                        // leading whitespace
                        while (*uninstall_script_name == ' ' || *uninstall_script_name == '\t') {
                            uninstall_script_name++;
                        }
                        // find end of script name without modifying the manifest buffer
                        char *end = uninstall_script_name;
                        while (*end != '\n' && *end != '#' && *end != '\0') {
                            end++;
                        }
                        size_t name_len = end - uninstall_script_name;
                        if (name_len > 0) {
                            char uninstall_name_buf[256];
                            size_t copy_len = (name_len < sizeof(uninstall_name_buf)-1) ? name_len : (sizeof(uninstall_name_buf)-1);
                            strncpy(uninstall_name_buf, uninstall_script_name, copy_len);
                            uninstall_name_buf[copy_len] = '\0';

                            char source_uninstall_script_path[512];
                            snprintf(source_uninstall_script_path, sizeof(source_uninstall_script_path), "%s/%s", untar_dir, uninstall_name_buf);

                            char dest_uninstall_script_path[512];
                            snprintf(dest_uninstall_script_path, sizeof(dest_uninstall_script_path), "%s/%s", pp_info_dir, uninstall_name_buf);

                            printf("Looking for uninstall script at: %s\n", source_uninstall_script_path);
                            FILE *source_uninstall_script = fopen(source_uninstall_script_path, "rb");
                            if (source_uninstall_script == NULL) {
                                perror("Error opening uninstall script");
                                printf("Uninstall script '%s' not found in package.\n", uninstall_name_buf);
                            } else {
                                printf("Saving uninstall script to: %s\n", dest_uninstall_script_path);
                                FILE *dest_uninstall_script = fopen(dest_uninstall_script_path, "wb");
                                if (dest_uninstall_script == NULL) {
                                    perror("Error saving uninstall script");
                                    fclose(source_uninstall_script);
                                } else {
                                    char script_buffer[4096];
                                    size_t script_bytes_read;
                                    while ((script_bytes_read = fread(script_buffer, 1, sizeof(script_buffer), source_uninstall_script)) > 0) {
                                        fwrite(script_buffer, 1, script_bytes_read, dest_uninstall_script);
                                    }
                                    fclose(source_uninstall_script);
                                    fclose(dest_uninstall_script);
                                    if (chmod(dest_uninstall_script_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) {
                                        printf("Uninstall script made executable in pp_info.\n");
                                    } else {
                                        perror("Error making uninstall script executable in pp_info");
                                    }
                                    printf("Uninstall script saved to pp_info.\n");
                                }
                            }
                        } else {
                            printf("Uninstall script specified in MANIFEST is empty.\n");
                        }
                    } else {
                        printf("No uninstall script specified in MANIFEST.\n");
                    }

                    /* New: parse a 'helper:' key in MANIFEST to copy additional helper files
                       Example: helper: uninstall-gcc-from-dir.sh uninstall.sh */
                    char *helpers_line = strstr(full_manifest_content, "helper:");
                    if (helpers_line != NULL) {
                        char *p = helpers_line + strlen("helper:");
                        // skip leading whitespace
                        while (*p == ' ' || *p == '\t') p++;
                        // read tokens until end of line
                        while (*p != '\0' && *p != '\n') {
                            char token[256];
                            int ti = 0;
                            // collect non-whitespace token
                            while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '#' && *p != '\0' && ti < (int)sizeof(token)-1) {
                                token[ti++] = *p++;
                            }
                            token[ti] = '\0';
                            if (ti > 0) {
                                char src_path[512];
                                char dst_path[512];
                                snprintf(src_path, sizeof(src_path), "%s/%s", untar_dir, token);
                                snprintf(dst_path, sizeof(dst_path), "%s/%s", pp_info_dir, token);
                                printf("Looking for helper file at: %s\n", src_path);
                                FILE *fh_src = fopen(src_path, "rb");
                                if (fh_src == NULL) {
                                    perror("Error opening helper file");
                                    printf("Helper file '%s' not found in package.\n", token);
                                } else {
                                    FILE *fh_dst = fopen(dst_path, "wb");
                                    if (fh_dst == NULL) {
                                        perror("Error saving helper file");
                                        fclose(fh_src);
                                    } else {
                                        char buf[4096]; size_t r;
                                        while ((r = fread(buf, 1, sizeof(buf), fh_src)) > 0) fwrite(buf, 1, r, fh_dst);
                                        fclose(fh_src); fclose(fh_dst);
                                        if (chmod(dst_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) {
                                            printf("Helper '%s' made executable in pp_info.\n", token);
                                        } else {
                                            perror("Error making helper executable");
                                        }
                                        printf("Helper '%s' saved to pp_info.\n", token);
                                    }
                                }
                            }
                            // skip whitespace to next token
                            while (*p == ' ' || *p == '\t') p++;
                        }
                    }
                }

                char *install_script_line = strstr(full_manifest_content, "install:");
                if (install_script_line != NULL) {
                    char *install_script_name = install_script_line + strlen("install:");
                    // trim whitespace
                    while (*install_script_name == ' ' || *install_script_name == '\t') {
                        install_script_name++;
                    }
                    // end of script name
                    char *end = install_script_name;
                    while (*end != '\n' && *end != '#' && *end != '\0') {
                        end++;
                    }
                    *end = '\0';

                    if (strlen(install_script_name) > 0) {
                        char install_script_relative_path[512];
                        snprintf(install_script_relative_path, sizeof(install_script_relative_path), "%s/%s", untar_dir, install_script_name);

                        printf("Looking for install script at: %s\n", install_script_relative_path);

                        char full_install_script_path[PATH_MAX];
                        if (realpath(install_script_relative_path, full_install_script_path) == NULL) {
                            perror("Error getting full path for install script");
                            printf("Could not get full path for install script '%s'. Cannot execute.\n", install_script_relative_path);
                        } else {
                             printf("Full install script path: %s\n", full_install_script_path);
                            if (chmod(full_install_script_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) {
                                 printf("Made install script executable.\n");
                                 printf("Executing install script: %s\n", full_install_script_path);
                                 int script_status = system(full_install_script_path);
                                if (script_status != 0) {
                                    printf("Error executing install script: script failed with status %d\n", script_status);
                                } else {
                                    printf("Install script execution complete.\n");
                                }
                            } else {
                                 perror("Error making install script executable");
                                printf("Could not make install script '%s' executable.\n", full_install_script_path);
                            }
                        }
                    } else {
                        printf("Install script specified in MANIFEST is empty.\n");
                    }
                } else {
                    printf("No install script specified in MANIFEST.\n");
                }
            }

            // TODO: clean up downloaded and untarred files in pp_download after

        } else if (strcmp(confirm_install, "N") == 0 || strcmp(confirm_install, "n") == 0) {
            printf("Skipping installation for %s.\n", package_name);
        } else {
            printf("Invalid input. Skipping installation for %s.\n", package_name);
        }
    } else {
        printf("Error reading confirmation input. Skipping installation for %s.\n", package_name);
    }
}


// remove a package
void remove_package(const char *package_name) {
    printf("Attempting to remove package: %s\n", package_name);

    char pp_info_dir[512];
    snprintf(pp_info_dir, sizeof(pp_info_dir), "pp_info/%s", package_name);

    struct stat st;
    if (stat(pp_info_dir, &st) == -1) {
        if (errno == ENOENT) {
            printf("Package '%s' not found in pp_info. It might not be installed.\n", package_name);
        } else {
            perror("Error checking package info directory");
        }
        return;
    }

    char confirm_remove[10];
    printf("Remove %s? (Y/n): ", package_name);
    if (fgets(confirm_remove, sizeof(confirm_remove), stdin) != NULL) {
        confirm_remove[strcspn(confirm_remove, "\n")] = 0;

        if (strlen(confirm_remove) == 0 ||
            strcmp(confirm_remove, "Y") == 0 || strcmp(confirm_remove, "y") == 0) {

            printf("Removing %s...\n", package_name);

            char manifest_path[512];
            snprintf(manifest_path, sizeof(manifest_path), "%s/MANIFEST", pp_info_dir);

            char full_manifest_content[4096] = ""; // Assuming MANIFEST is not larger than 4KB
            FILE *manifest_file = fopen(manifest_path, "r");
            if (manifest_file == NULL) {
                perror("Error opening MANIFEST file in pp_info");
                printf("MANIFEST file not found for package '%s' in pp_info. Cannot execute uninstall script.\n", package_name);
            } else {
                char manifest_line[256];
                while (fgets(manifest_line, sizeof(manifest_line), manifest_file)) {
                    strncat(full_manifest_content, manifest_line, sizeof(full_manifest_content) - strlen(full_manifest_content) - 1);
                }
                fclose(manifest_file);

                // parse MANIFEST
                char *uninstall_script_line = strstr(full_manifest_content, "uninstall:");
                char uninstall_script_name[256] = "";
                char uninstall_script_path[512] = "";

                if (uninstall_script_line != NULL) {
                    char *temp_script_name = uninstall_script_line + strlen("uninstall:");
                    // trim whitespace
                    while (*temp_script_name == ' ' || *temp_script_name == '\t') {
                        temp_script_name++;
                    }
                    // end of script name
                    char *end = temp_script_name;
                    while (*end != '\n' && *end != '#' && *end != '\0') {
                        end++;
                    }
                    size_t name_len = end - temp_script_name;
                    if (name_len > 0) {
                        // copy only the name_len characters into uninstall_script_name
                        size_t copy_len = (name_len < sizeof(uninstall_script_name) - 1) ? name_len : (sizeof(uninstall_script_name) - 1);
                        strncpy(uninstall_script_name, temp_script_name, copy_len);
                        uninstall_script_name[copy_len] = '\0';
                        snprintf(uninstall_script_path, sizeof(uninstall_script_path), "%s/%s", pp_info_dir, uninstall_script_name);

                        printf("Looking for uninstall script at: %s\n", uninstall_script_path);

                        char full_uninstall_script_path[PATH_MAX];
                        if (realpath(uninstall_script_path, full_uninstall_script_path) == NULL) {
                             perror("Error getting full path for uninstall script");
                            printf("Could not get full path for uninstall script '%s'. Cannot execute.\n", uninstall_script_path);
                        } else {
                             printf("Full uninstall script path: %s\n", full_uninstall_script_path);

                             if (chmod(full_uninstall_script_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) {
                                 printf("Made uninstall script executable.\n");
                                 printf("Executing uninstall script: %s\n", full_uninstall_script_path);
                                 int script_status = system(full_uninstall_script_path);
                                 if (script_status != 0) {
                                     printf("Error executing uninstall script: script failed with status %d\n", script_status);
                                 } else {
                                     printf("Uninstall script execution complete.\n");
                                 }
                             } else {
                                  perror("Error making uninstall script executable");
                                 printf("Could not make uninstall script '%s' executable.\n", full_uninstall_script_path);
                             }
                        }
                    } else {
                        printf("Uninstall script specified in MANIFEST is empty.\n");
                    }
                } else {
                    printf("No uninstall script specified in MANIFEST.\n");
                }

                // remove the MANIFEST file and the uninstall script (if it exists).
                char saved_manifest_path[512];
                snprintf(saved_manifest_path, sizeof(saved_manifest_path), "%s/MANIFEST", pp_info_dir);
                printf("Removing MANIFEST file: %s\n", saved_manifest_path);
                if (remove(saved_manifest_path) != 0) {
                    perror("Error removing MANIFEST file");
                } else {
                    printf("MANIFEST file removed.\n");
                }

                if (strlen(uninstall_script_name) > 0) {
                    char uninstall_script_full_path_to_remove[PATH_MAX];
                     if (realpath(uninstall_script_path, uninstall_script_full_path_to_remove) == NULL) {
                         perror("Error getting full path for uninstall script to remove");
                        printf("Could not get full path for uninstall script '%s'. May require manual removal.\n", uninstall_script_path);
                     } else {
                         printf("Removing uninstall script: %s\n", uninstall_script_full_path_to_remove);
                         if (remove(uninstall_script_full_path_to_remove) != 0) {
                             perror("Error removing uninstall script");
                         } else {
                             printf("Uninstall script removed.\n");
                         }
                     }
                }

                // Also remove any helper files listed in MANIFEST under 'helper:'
                char *helpers_line_rem = strstr(full_manifest_content, "helper:");
                if (helpers_line_rem != NULL) {
                    char *p = helpers_line_rem + strlen("helper:");
                    while (*p == ' ' || *p == '\t') p++;
                    while (*p != '\0' && *p != '\n') {
                        char token[256]; int ti = 0;
                        while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '#' && *p != '\0' && ti < (int)sizeof(token)-1) {
                            token[ti++] = *p++;
                        }
                        token[ti] = '\0';
                        if (ti > 0) {
                            char helper_path[512];
                            snprintf(helper_path, sizeof(helper_path), "%s/%s", pp_info_dir, token);
                            printf("Removing helper file: %s\n", helper_path);
                            if (remove(helper_path) != 0) {
                                perror("Error removing helper file");
                            } else {
                                printf("Helper file removed: %s\n", helper_path);
                            }
                        }
                        while (*p == ' ' || *p == '\t') p++;
                    }
                }
            }

            // remove the pp_info/PACKAGENAME/ directory
            printf("Removing package info directory: %s\n", pp_info_dir);
            if (rmdir(pp_info_dir) == -1) {
                 perror("Error removing package info directory");
                 printf("Directory might not be empty after uninstall. You might need to manually remove the directory: %s\n", pp_info_dir);
            } else {
                printf("Package info directory removed.\n");
            }

        } else if (strcmp(confirm_remove, "N") == 0 || strcmp(confirm_remove, "n") == 0) {
            printf("Skipping removal for %s.\n", package_name);
        } else {
            printf("Invalid input. Skipping removal for %s.\n", package_name);
        }
    } else {
        printf("Error reading confirmation input. Skipping removal for %s.\n", package_name);
    }
}


// upgrade packages that are installed locally (present in pp_info) but have a newer version available.
void upgrade_packages(int filter_flag) {
    printf("Checking for upgrades%s...\n", (filter_flag != -1) ? " with flag filter" : "");

    // TODO: lu command for this
    printf("Updating local system metadata...\n");
    read_local_package_list();
    read_repository_package_list();
    write_local_package_list();
    printf("Local system metadata updated.\n");

    printf("Identifying upgradable packages...\n");
    for (int i = 0; i < local_package_count; i++) {
        // check if the package is installed
        char pp_info_dir[512];
        snprintf(pp_info_dir, sizeof(pp_info_dir), "pp_info/%s", local_packages[i].name);

        struct stat st;
        if (stat(pp_info_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            // Package is installed, now check for a newer version

            // read the version from the saved MANIFEST in pp_info
            char manifest_path_in_info[512];
            snprintf(manifest_path_in_info, sizeof(manifest_path_in_info), "%s/MANIFEST", pp_info_dir);

            FILE *manifest_file_in_info = fopen(manifest_path_in_info, "r");
            char installed_version[20] = ""; // Buffer for installed version

            if (manifest_file_in_info != NULL) { // TODO: function to read the manifest maybe?
                char manifest_line[256];
                while (fgets(manifest_line, sizeof(manifest_line), manifest_file_in_info)) {
                    if (strstr(manifest_line, "version:") != NULL) {
                        char *version_str = strstr(manifest_line, "version:") + strlen("version:");
                        // Trim leading whitespace
                        while (*version_str == ' ' || *version_str == '\t') {
                            version_str++;
                        }
                        // end of version string
                        char *end = version_str;
                        while (*end != '\n' && *end != '#' && *end != '\0') {
                            end++;
                        }

                        // null-terminate the version string at the end
                        *end = '\0';

                        size_t version_len = end - version_str;
                        if (version_len > 0) {
                            strncpy(installed_version, version_str, sizeof(installed_version) - 1);
                            installed_version[sizeof(installed_version) - 1] = '\0';
                        }
                        break;
                    }
                }
                fclose(manifest_file_in_info);
            }

            // TODO: compare versions
            if (strlen(installed_version) > 0 && strcmp(local_packages[i].version, installed_version) > 0) {
                if (filter_flag == -1 || local_packages[i].package_status == filter_flag) {
                    printf("Upgrade available for %s (Installed: %s, Available: %s)\n", 
                            local_packages[i].name, 
                            installed_version, 
                            local_packages[i].version);
                    char confirm_upgrade[10];
                    printf("Upgrade %s? (Y/n): ", local_packages[i].name);
                    if (fgets(confirm_upgrade, sizeof(confirm_upgrade), stdin) != NULL) {
                        confirm_upgrade[strcspn(confirm_upgrade, "\n")] = 0;

                        if (strlen(confirm_upgrade) == 0 ||
                            strcmp(confirm_upgrade, "Y") == 0 || strcmp(confirm_upgrade, "y") == 0) {

                            printf("Upgrading %s...\n", local_packages[i].name);

                            // uninstall the old version and install the new one.
                            char full_manifest_content_in_info[4096] = "";
                            char uninstall_script_name[256] = "";

                            manifest_file_in_info = fopen(manifest_path_in_info, "r");

                            if (manifest_file_in_info != NULL) {
                                char manifest_line_in_info[256];
                                while (fgets(manifest_line_in_info, sizeof(manifest_line_in_info), manifest_file_in_info)) {
                                    strncat(full_manifest_content_in_info, manifest_line_in_info, sizeof(full_manifest_content_in_info) - strlen(full_manifest_content_in_info) - 1);
                                }
                                fclose(manifest_file_in_info);

                                char *uninstall_script_line = strstr(full_manifest_content_in_info, "uninstall:");
                                if (uninstall_script_line != NULL) {
                                    char *temp_script_name = uninstall_script_line + strlen("uninstall:");

                                    // trim whitespace
                                    while (*temp_script_name == ' ' || *temp_script_name == '\t') {
                                        temp_script_name++;
                                    }

                                    // end of script name
                                    char *end = temp_script_name;
                                    while (*end != '\n' && *end != '#' && *end != '\0') {
                                        end++;
                                    }
                                    size_t name_len = end - temp_script_name;
                                    if (name_len > 0) {
                                        strncpy(uninstall_script_name, temp_script_name, sizeof(uninstall_script_name) - 1);
                                        uninstall_script_name[sizeof(uninstall_script_name) - 1] = '\0';
                                    }
                                }
                            } else {
                                perror("Error reading MANIFEST for old version uninstall script");
                            }


                            if (strlen(uninstall_script_name) > 0) {
                                char uninstall_script_relative_path[512];
                                snprintf(uninstall_script_relative_path, sizeof(uninstall_script_relative_path), "%s/%s", pp_info_dir, uninstall_script_name);
                                printf("Executing uninstall script for old version: %s\n", uninstall_script_relative_path);

                                char full_uninstall_script_path[PATH_MAX];
                                if (realpath(uninstall_script_relative_path, full_uninstall_script_path) == NULL) {
                                    perror("Error getting full path for uninstall script");
                                    printf("Could not get full path for uninstall script '%s'. Skipping uninstall.\n", uninstall_script_relative_path);
                                } else {
                                    printf("Full uninstall script path: %s\n", full_uninstall_script_path);
                                    if (chmod(full_uninstall_script_path, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0) {
                                        printf("Made uninstall script executable.\n");
                                        int script_status = system(full_uninstall_script_path);
                                        if (script_status != 0) {
                                            printf("Error executing uninstall script for old version: script failed with status %d\n", script_status);
                                        } else {
                                            printf("Uninstall script for old version executed successfully.\n");
                                        }
                                    } else {
                                        perror("Error making uninstall script executable");
                                        printf("Could not make uninstall script '%s' executable. Skipping uninstall.\n", full_uninstall_script_path);
                                    }
                                }
                            } else {
                                printf("No uninstall script specified in MANIFEST for old version.\n");
                            }

                            // elements to remove, manifest and uninstall script, directory
                            printf("Removing package info directory for old version: %s\n", pp_info_dir);
                            char old_manifest_path[512];
                            snprintf(old_manifest_path, sizeof(old_manifest_path), "%s/MANIFEST", pp_info_dir);
                            printf("Removing old MANIFEST file: %s\n", old_manifest_path);
                            if (remove(old_manifest_path) != 0) {
                                perror("Error removing old MANIFEST file");
                            } else {
                                printf("Old MANIFEST file removed.\n");
                            }

                            if (strlen(uninstall_script_name) > 0) {
                                char old_uninstall_script_path[512];
                                snprintf(old_uninstall_script_path, sizeof(old_uninstall_script_path), "%s/%s", pp_info_dir, uninstall_script_name);
                                printf("Removing old uninstall script: %s\n", old_uninstall_script_path);
                                if (remove(old_uninstall_script_path) != 0) {
                                    perror("Error removing old uninstall script");
                                } else {
                                    printf("Old uninstall script removed.\n");
                                }
                            }

                            if (rmdir(pp_info_dir) == -1) {
                                perror("Error removing package info directory for old version");
                                printf("Directory might not be empty after uninstall. You might need to manually remove the directory: %s\n", pp_info_dir);
                            } else {
                                printf("Package info directory for old version removed.\n");
                            }
                            printf("Installing new version of %s...\n", local_packages[i].name);
                            install_package(local_packages[i].name);
                        } else if (strcmp(confirm_upgrade, "N") == 0 || strcmp(confirm_upgrade, "n") == 0) {
                            printf("Skipping upgrade for %s.\n", local_packages[i].name);
                        } else {
                            printf("Invalid input. Skipping upgrade for %s.\n", local_packages[i].name);
                        }
                    } else {
                        printf("Error reading confirmation input. Skipping upgrade for %s.\n", local_packages[i].name);
                    }
                }
            } else {
                printf("Package %s is installed and up to date (Version: %s).\n",
                       local_packages[i].name, local_packages[i].version);
            }
        } else {
            // TODO: package is not installed locally, check if it is available
        }
    }
    printf("Upgrade check complete.\n");
}

// update a specific package
void update_package(const char *package_name) {
    printf("Attempting to update package: %s \n", package_name);

    // TODO: pass param to force update or not the local package list
    read_local_package_list(); // Load local package list
    read_repository_package_list(); // Update local list with repository info

    int package_index = find_local_package(package_name);

    if (package_index == -1) {
        printf("Error: Package \'%s\' not found in local package list. Cannot update.\n", package_name);
        return;
    }

    // check if the package is installed locally (exists in pp_info)
    char pp_info_dir[512];
    snprintf(pp_info_dir, sizeof(pp_info_dir), "pp_info/%s", package_name);
    struct stat st;
    int is_installed = (stat(pp_info_dir, &st) == 0 && S_ISDIR(st.st_mode));

    if (!is_installed) {
        printf("Package \'%s\' is not installed. Cannot update.\n", package_name);
        return;
    }

    // read installed version from MANIFEST in pp_info
    char manifest_path_in_info[512];
    snprintf(manifest_path_in_info, sizeof(manifest_path_in_info), "%s/MANIFEST", pp_info_dir);
    FILE *manifest_file_in_info = fopen(manifest_path_in_info, "r");
    char installed_version[20] = ""; // buffer for installed version

    if (manifest_file_in_info != NULL) {
        char manifest_line[256];
        while (fgets(manifest_line, sizeof(manifest_line), manifest_file_in_info)) {
            if (strstr(manifest_line, "version:") != NULL) {
                char *version_str = strstr(manifest_line, "version:") + strlen("version:");
                // trim whitespace
                while (*version_str == ' ' || *version_str == '\t') {
                    version_str++;
                }
                // find end of version string, excluding newline
                char *end = version_str;
                while (*end != '\n' && *end != '#' && *end != '\0') {
                    end++;
                }
                // null-terminate the version string at the end
                *end = '\0'; // remove next line

                size_t version_len = strlen(version_str); // use strlen after null-termination
                if (version_len > 0) {
                    strncpy(installed_version, version_str, sizeof(installed_version) - 1);
                    installed_version[sizeof(installed_version) - 1] = '\0';
                }
                break;
            }
        }
        fclose(manifest_file_in_info);
    } else {
        perror("Error opening MANIFEST file in pp_info");
        printf("Cannot read installed version for package \'%s\'. Cannot update.\n", package_name);
        return;
    }

    // compare versions
    if (strcmp(local_packages[package_index].version, installed_version) > 0) {
        printf("Upgrade available for %s (Installed: %s, Available: %s)\n",
               package_name,
               installed_version,
               local_packages[package_index].version);

        char confirm_upgrade[10];
        printf("Upgrade %s? (Y/n): ", package_name);
        if (fgets(confirm_upgrade, sizeof(confirm_upgrade), stdin) != NULL) {
            confirm_upgrade[strcspn(confirm_upgrade, "\n")] = 0;

            if (strlen(confirm_upgrade) == 0 ||
                strcmp(confirm_upgrade, "Y") == 0 || strcmp(confirm_upgrade, "y") == 0) {

                printf("Upgrading %s...\n", package_name);
                remove_package(package_name); // uninstall old version TODO: pass Y
                install_package(package_name);  // install new version TODO: pass Y
                printf("Upgrade of %s complete.\n", package_name);
            } else if (strcmp(confirm_upgrade, "N") == 0 || strcmp(confirm_upgrade, "n") == 0) {
                printf("Skipping upgrade for %s.\n", package_name);
            } else {
                printf("Invalid input. Skipping upgrade for %s.\n", package_name);
            }
        } else {
            printf("Error reading confirmation input. Skipping upgrade for %s.\n", package_name);
        }

    } else {
        printf("Package %s is already up to date (Version: %s).\n", package_name, installed_version);
    }
}

// add a package manually
void add_package_manual(const char *package_name, const char *version, const char *url, const char *sha256) {
    printf("Attempting to add package manually: %s version %s from %s with SHA256 %s\n", package_name, version, url, sha256);

    read_local_package_list(); // load the current local package list

    // check if the package already exists in the local list
    int existing_index = find_local_package(package_name);
    if (existing_index != -1) {
        printf("Error: Package \'%s\' already exists in the local package list. Cannot add.\n", package_name);
        return;
    }

    // dynamic allocation for the new package
    if (local_package_count >= allocated_packages) {
        int new_size = (allocated_packages == 0) ? 10 : allocated_packages * 2; // start with 10, then double
        Package *temp = realloc(local_packages, new_size * sizeof(Package));
        if (temp == NULL) {
            perror("Error reallocating memory for local packages");
            return;
        }
        local_packages = temp;
        allocated_packages = new_size;
    }

    // copy the package information into the new entry
    strncpy(local_packages[local_package_count].name, package_name, sizeof(local_packages[0].name) - 1);
    local_packages[local_package_count].name[sizeof(local_packages[0].name) - 1] = '\0';
    strncpy(local_packages[local_package_count].version, version, sizeof(local_packages[0].version) - 1);
    local_packages[local_package_count].version[sizeof(local_packages[0].version) - 1] = '\0';
    strncpy(local_packages[local_package_count].url, url, sizeof(local_packages[0].url) - 1);
    local_packages[local_package_count].url[sizeof(local_packages[0].url) - 1] = '\0';
    strncpy(local_packages[local_package_count].sha256, sha256, sizeof(local_packages[0].sha256) - 1);
    local_packages[local_package_count].sha256[sizeof(local_packages[0].sha256) - 1] = '\0';

    local_packages[local_package_count].package_status = MANUAL_PKG_FLAG; // set the manual package flag
    local_packages[local_package_count].present_in_repository = 0; // not from the repository(only in local)

    local_package_count++;

    write_local_package_list();

    printf("Package \'%s\' added to the local package list.\n", package_name);
}

// list packages by flag
void list_packages_by_flag(int flag) {
    printf("Listing packages with flag %d:\n", flag);

    read_local_package_list(); // load the current local package list

    int found_count = 0;
    for (int i = 0; i < local_package_count; i++) {
        if (local_packages[i].package_status == flag) {
            printf("  Name: %s, Version: %s, SHA256: %s, URL: %s, Status: %d\n",
                   local_packages[i].name,
                   local_packages[i].version,
                   local_packages[i].sha256,
                   local_packages[i].url,
                   local_packages[i].package_status);
            found_count++;
        }
    }

    if (found_count == 0) {
        printf("No packages found with flag %d.\n", flag);
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: pp [command] [package_name]\n");
        return 1;
    }

    char *command = argv[1];
    char *package_name = NULL;

    if (argc > 2) {
        package_name = argv[2];
    }

    printf("Command: %s\n", command);
    if (package_name) {
        printf("Package Name: %s\n", package_name);
    }

    if (strcmp(command, "lu") == 0) {
        printf("Updating local system metadata...\n");
        read_local_package_list();
        read_repository_package_list();
        write_local_package_list(); // pp_pkg_list
    } else if (strcmp(command, "s") == 0) {
        if (package_name == NULL) {
            printf("Usage: pp s [search_term]\n");
            return 1;
        }
        read_local_package_list();
        search_package(package_name);
    } else if (strcmp(command, "e") == 0) {
        if (package_name == NULL) {
            printf("Usage: pp e [package_name]\n");
            return 1;
        }
        read_local_package_list();
        exact_search_package(package_name);
    } else if (strcmp(command, "i") == 0) {
         if (package_name == NULL) {
            printf("Usage: pp i [package_name]\n");
            return 1;
        }
        install_package(package_name);
    } else if (strcmp(command, "r") == 0) {
         if (package_name == NULL) {
            printf("Usage: pp r [package_name]\n");
            return 1;
        }
        remove_package(package_name);
    } else if (strcmp(command, "up") == 0) {
        int filter_flag = -1; // default to no filter
        if (argc > 2) {
            char *flag_arg = argv[2];
            char *endptr;
            long flag_value = strtol(flag_arg, &endptr, 10);

            if (*endptr == '\0' && endptr != flag_arg) {
                filter_flag = (int)flag_value;
            } else {
                printf("Warning: Invalid flag argument for 'up' command. Upgrading all applicable packages.\n");
                // keep filter_flag as -1 for no filter
            }
        }
        upgrade_packages(filter_flag); // Pass the filter flag
    } else if (strcmp(command, "u") == 0) {
        if (package_name == NULL) {
            printf("Usage: pp u [package_name]\n");
            return 1;
        }
        update_package(package_name);
    } else if (strcmp(command, "a") == 0) {
        if (argc < 6) { // need command, package_name, version, url, and sha256
            printf("Usage: pp a PACKAGENAME VERSION LOCAL_PATH/URL SHA256\n");
            return 1;
        }
        char *package_name_a = argv[2];
        char *version_a = argv[3];
        char *url_a = argv[4];
        char *sha256_a = argv[5]; // Get the SHA256 argument
        add_package_manual(package_name_a, version_a, url_a, sha256_a);
    } else if (strcmp(command, "l") == 0) {
        if (argc < 3) {
            printf("Usage: pp l FLAG\n");
            return 1;
        }
        int flag_to_list = atoi(argv[2]); // convert the flag argument to an integer for now
        list_packages_by_flag(flag_to_list);
    }
    else {
        printf("Unknown command: %s\n", command);
        printf("Usage: pp [command] [package_name]\n");
        printf("Usage: pp [i|r|s|e|u] PACKAGENAME | pp a PACKAGENAME VERSION LOCAL_PATH/URL SHA256 | pp l FLAG | pp [up [FLAG]|lu]\n");
        return 1;
    }


    // free allocated memory before exiting
    if (local_packages != NULL) {
        free(local_packages);
        local_packages = NULL; // set to NULL after freeing
    }

    return 0;
}