/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "private/android_filesystem_config.h"

#include "canned_fs_config.h"

typedef struct {
	const char* path;
	unsigned uid;
	unsigned gid;
	unsigned mode;
	uint64_t capabilities;
} Path;

static Path* canned_data = NULL;
static int canned_alloc = 0;
static int canned_used = 0;

static int path_compare(const void* a, const void* b) {
	return strcmp(((Path*)a)->path, ((Path*)b)->path);
}

int load_canned_fs_config(const char* fn) {
	FILE* f = fopen(fn, "r");
	if (f == NULL) {
		fprintf(stderr, "failed to open %s: %s\n", fn, strerror(errno));
		return -1;
	}

	char line[PATH_MAX + 200];
	while (fgets(line, sizeof(line), f)) {
		while (canned_used >= canned_alloc) {
			canned_alloc = (canned_alloc+1) * 2;
			canned_data = (Path*) realloc(canned_data, canned_alloc * sizeof(Path));
		}
		Path* p = canned_data + canned_used;
		p->path = strdup(strtok(line, " "));
		p->uid = atoi(strtok(NULL, " "));
		p->gid = atoi(strtok(NULL, " "));
		p->mode = strtol(strtok(NULL, " "), NULL, 8);   // mode is in octal
		p->capabilities = 0;

		char* token = NULL;
		do {
			token = strtok(NULL, " ");
			if (token && strncmp(token, "capabilities=", 13) == 0) {
				p->capabilities = strtoll(token+13, NULL, 0);
				break;
			}
		} while (token);

		canned_used++;
	}

	fclose(f);

	qsort(canned_data, canned_used, sizeof(Path), path_compare);
	printf("loaded %d fs_config entries\n", canned_used);

	return 0;
}

void canned_fs_config(const char* path, int dir,
					  unsigned* uid, unsigned* gid, unsigned* mode, uint64_t* capabilities) {
	Path key;
	key.path = path+1;   // canned paths lack the leading '/'
	Path* p = (Path*) bsearch(&key, canned_data, canned_used, sizeof(Path), path_compare);
	if (p == NULL) {
		fprintf(stderr, "cannot find [%s] in canned fs_config\nAuto Fixing...", path);
		struct stat buf;
		int result;
		result = stat(path, &buf);
		*uid = "0";
		*gid = "0";
		if(S_IFDIR & buf.st_mode){
			fprintf(stderr, "folder: %s 0755\n", path);
			unsigned* mode = "0755";
			/*Auto fix folder*/
		}else if(S_IFREG & buf.st_mode){
			fprintf(stderr, "file: %s 0644\n", path);
			unsigned* mode = "0644";
			/*Auto fix file*/
		}
	*capabilities = "0";
	} else {
	*uid = p->uid;
	*gid = p->gid;
	*mode = p->mode;
	*capabilities = p->capabilities;}

#if 0
	// for debugging, run the built-in fs_config and compare the results.

	unsigned c_uid, c_gid, c_mode;
	uint64_t c_capabilities;
	fs_config(path, dir, &c_uid, &c_gid, &c_mode, &c_capabilities);

	if (c_uid != *uid) printf("%s uid %d %d\n", path, *uid, c_uid);
	if (c_gid != *gid) printf("%s gid %d %d\n", path, *gid, c_gid);
	if (c_mode != *mode) printf("%s mode 0%o 0%o\n", path, *mode, c_mode);
	if (c_capabilities != *capabilities) printf("%s capabilities %llx %llx\n", path, *capabilities, c_capabilities);
#endif
}
