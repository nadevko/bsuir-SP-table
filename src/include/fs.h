#pragma once
/* fs.h */

int traverse_fs(void *arg);

/* Render header template with substitutions (%P, %p, %b, %f, %d, %%)
 * Returns malloc'd string (caller must free) */
char *render_header_template(const char *tmpl);