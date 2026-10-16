
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONFIG_FILE ".config"
#define BUILD_DIR   "build/config"
#define PATH_SIZE   4096
#define LINE_SIZE   4096

static int enabled(const char *symbol)
{
    FILE *file = fopen(CONFIG_FILE, "r");
    char line[LINE_SIZE];
    int result = 0;

    if (!file) {
        perror(CONFIG_FILE);
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);

        while (len > 0 &&
               (line[len - 1] == '\n' ||
                line[len - 1] == '\r' ||
                line[len - 1] == ' ' ||
                line[len - 1] == '\t')) {
            line[--len] = '\0';
        }

        if (strncmp(line, symbol, strlen(symbol)) == 0 &&
            line[strlen(symbol)] == '=' &&
            strcmp(line + strlen(symbol) + 1, "y") == 0) {
            result = 1;
            break;
        }
    }

    fclose(file);
    return result;
}

static int make_directories(const char *path)
{
    char buffer[PATH_SIZE];
    size_t len;

    if (strlen(path) >= sizeof(buffer)) {
        fprintf(stderr, "oconf: path too long: %s\n", path);
        return -1;
    }

    strcpy(buffer, path);
    len = strlen(buffer);

    for (size_t i = 1; i < len; i++) {
        if (buffer[i] != '/')
            continue;

        buffer[i] = '\0';

        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
            perror(buffer);
            return -1;
        }

        buffer[i] = '/';
    }

    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        perror(buffer);
        return -1;
    }

    return 0;
}

static int compile_source(const char *source)
{
    char output[PATH_SIZE];
    char directory[PATH_SIZE];
    char *slash;
    size_t len;
    pid_t pid;
    int status;

    len = strlen(source);

    if (len < 3 || strcmp(source + len - 2, ".c") != 0) {
        fprintf(stderr,
                "oconf: source must end in .c: %s\n", source);
        return -1;
    }

    if (snprintf(output, sizeof(output), "%s/%s",
                 BUILD_DIR, source) >= (int)sizeof(output)) {
        fprintf(stderr, "oconf: output path too long\n");
        return -1;
    }

    /* Change the final .c extension to .o. */
    output[strlen(output) - 1] = 'o';

    if (strlen(output) >= sizeof(directory)) {
        fprintf(stderr, "oconf: directory path too long\n");
        return -1;
    }

    strcpy(directory, output);
    slash = strrchr(directory, '/');

    if (slash)
        *slash = '\0';

    if (make_directories(directory) != 0)
        return -1;

    if (access(source, R_OK) != 0) {
        fprintf(stderr, "oconf: source not found: %s\n", source);
        return -1;
    }

    printf("  CC      %s\n", source);
    fflush(stdout);

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execlp("gcc", "gcc",
               "-m32",
               "-std=gnu99",
               "-ffreestanding",
               "-fno-builtin",
               "-O2",
               "-Wall",
               "-Wextra",
               "-fno-pie",
               "-Iinclude",
               "-c", source,
               "-o", output,
               (char *)NULL);

        perror("gcc");
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "oconf: compilation failed: %s\n", source);
        return -1;
    }

    return 0;
}

static int has_suffix(const char *name, const char *suffix)
{
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);

    return name_len >= suffix_len &&
           strcmp(name + name_len - suffix_len, suffix) == 0;
}

static int process_ocfg(const char *path)
{
    FILE *file;
    char line[LINE_SIZE];
    char config[256] = "";
    char directory[PATH_SIZE];
    char source_path[PATH_SIZE];
    char *slash;
    int source_count = 0;
    int result;

    file = fopen(path, "r");

    if (!file) {
        perror(path);
        return -1;
    }

    if (strlen(path) >= sizeof(directory)) {
        fprintf(stderr, "oconf: .Ocfg path too long: %s\n", path);
        fclose(file);
        return -1;
    }

    strcpy(directory, path);
    slash = strrchr(directory, '/');

    if (slash)
        *slash = '\0';
    else
        strcpy(directory, ".");

    printf("  OCFG    %s\n", path);

    while (fgets(line, sizeof(line), file)) {
        char *key;
        char *value;
        char *equals;
        size_t len = strlen(line);

        while (len > 0 &&
               (line[len - 1] == '\n' ||
                line[len - 1] == '\r' ||
                line[len - 1] == ' ' ||
                line[len - 1] == '\t')) {
            line[--len] = '\0';
        }

        key = line;

        while (*key == ' ' || *key == '\t')
            key++;

        if (*key == '\0' || *key == '#')
            continue;

        equals = strchr(key, '=');

        if (!equals) {
            fprintf(stderr,
                    "oconf: invalid line in %s: %s\n",
                    path, key);
            fclose(file);
            return -1;
        }

        *equals = '\0';
        value = equals + 1;

        while (*value == ' ' || *value == '\t')
            value++;

        if (strcmp(key, "CONFIG") == 0) {
            if (strncmp(value, "CONFIG_", 7) != 0) {
                fprintf(stderr,
                        "oconf: invalid CONFIG symbol in %s: %s\n",
                        path, value);
                fclose(file);
                return -1;
            }

            if (strlen(value) >= sizeof(config)) {
                fprintf(stderr, "oconf: CONFIG symbol too long\n");
                fclose(file);
                return -1;
            }

            strcpy(config, value);
        } else if (strcmp(key, "SOURCE") == 0) {
            if (config[0] == '\0') {
                fprintf(stderr,
                        "oconf: CONFIG must appear before SOURCE in %s\n",
                        path);
                fclose(file);
                return -1;
            }

            if (*value == '\0' || value[0] == '/') {
                fprintf(stderr,
                        "oconf: SOURCE must be a relative path in %s\n",
                        path);
                fclose(file);
                return -1;
            }

            if (snprintf(source_path, sizeof(source_path),
                         "%s/%s", directory, value)
                >= (int)sizeof(source_path)) {
                fprintf(stderr, "oconf: source path too long\n");
                fclose(file);
                return -1;
            }

            result = enabled(config);

            if (result < 0) {
                fclose(file);
                return -1;
            }

            if (result == 1) {
                if (compile_source(source_path) != 0) {
                    fclose(file);
                    return -1;
                }

                source_count++;
            }
        } else {
            fprintf(stderr,
                    "oconf: unknown field '%s' in %s\n",
                    key, path);
            fclose(file);
            return -1;
        }
    }

    if (ferror(file)) {
        perror(path);
        fclose(file);
        return -1;
    }

    fclose(file);

    if (config[0] == '\0') {
        fprintf(stderr, "oconf: missing CONFIG entry in %s\n", path);
        return -1;
    }

    result = enabled(config);

    if (result < 0)
        return -1;

    if (result == 0)
        printf("  SKIP    %s is not enabled\n", config);
    else if (source_count == 0)
        printf("  NOTE    %s has no SOURCE entries\n", config);

    return 0;
}

static int ignored_directory(const char *name)
{
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, "build") == 0;
}

static int scan_directory(const char *directory)
{
    DIR *dir;
    struct dirent *entry;
    char path[PATH_SIZE];
    struct stat st;
    int result = 0;

    dir = opendir(directory);

    if (!dir) {
        perror(directory);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (ignored_directory(entry->d_name))
            continue;

        if (snprintf(path, sizeof(path), "%s/%s",
                     directory, entry->d_name) >= (int)sizeof(path)) {
            fprintf(stderr, "oconf: path too long\n");
            result = -1;
            break;
        }

        if (lstat(path, &st) != 0) {
            perror(path);
            result = -1;
            break;
        }

        /* Do not follow directory symlinks. */
        if (S_ISDIR(st.st_mode)) {
            if (scan_directory(path) != 0) {
                result = -1;
                break;
            }
        } else if (S_ISREG(st.st_mode) &&
                   has_suffix(entry->d_name, ".Ocfg")) {
            if (process_ocfg(path) != 0) {
                result = -1;
                break;
            }
        }
    }

    closedir(dir);
    return result;
}

int main(int argc, char **argv)
{
    const char *root = ".";

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [repository-root]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2)
        root = argv[1];

    if (access(CONFIG_FILE, R_OK) != 0) {
        fprintf(stderr,
                "oconf: cannot read %s from the current directory\n",
                CONFIG_FILE);
        return EXIT_FAILURE;
    }

    printf("oconf: scanning %s for .Ocfg files\n", root);

    if (scan_directory(root) != 0)
        return EXIT_FAILURE;

    printf("oconf: configuration build complete\n");
    return EXIT_SUCCESS;
}
