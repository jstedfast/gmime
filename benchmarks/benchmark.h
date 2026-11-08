/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2026 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <gmime/gmime.h>

typedef guint64 (*BenchmarkFunc) (gpointer data);

gboolean benchmark_selected (const char *name);
void benchmark_run (const char *name, gsize bytes, BenchmarkFunc func, gpointer data);
GBytes *benchmark_load (const char *directory, const char *name);
GBytes *benchmark_newlines (GBytes *bytes, gboolean dos);
void benchmark_equal (GBytes *actual, GBytes *expected, const char *name);
void benchmark_check (gboolean success, const char *operation, GError **error);
void benchmark_headers (void);
void benchmark_parser (const char *datadir, const char *testdatadir);
void benchmark_codecs (const char *datadir, const char *testdatadir);

#endif
