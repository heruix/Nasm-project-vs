/* ----------------------------------------------------------------------- *
 *
 *   Copyright 1996-2016 The NASM Authors - All Rights Reserved
 *   See the file AUTHORS included with the NASM distribution for
 *   the specific copyright holders.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following
 *   conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ----------------------------------------------------------------------- */

/*
 * The Netwide Assembler main program module
 */


#include "common.h"

int main(int argc, char **argv)
{
	StrList *depend_list = NULL, **depend_ptr;

	time(&official_compile_time);

	iflag_set(&cpu, IF_PLEVEL);
	iflag_set(&cmd_cpu, IF_PLEVEL);

	pass0 = 0;
	want_usage = terminate_after_phase = false;
	nasm_set_verror(nasm_verror_gnu);

	error_file = stderr;

	tolower_init();

	offsets = raa_init();
	forwrefs = saa_init((int32_t)sizeof(struct forwrefinfo));

	preproc = &nasmpp;
	operating_mode = OP_NORMAL;

	/* Define some macros dependent on the runtime, but not
	on the command line. */
	define_macros_early();

	parse_cmdline(argc, argv);

	if (terminate_after_phase) {
		if (want_usage)
			usage();
		return 1;
	}

	/* If debugging info is disabled, suppress any debug calls */
	if (!using_debug_info)
		ofmt->current_dfmt = &null_debug_form;

	if (ofmt->stdmac)
		preproc->extra_stdmac(ofmt->stdmac);
	parser_global_info(&location);
	eval_global_info(ofmt, lookup_label, &location);

	/* define some macros dependent of command-line */
	define_macros_late();

	depend_ptr = (depend_file || (operating_mode & OP_DEPEND)) ? &depend_list : NULL;
	if (!depend_target)
		depend_target = quote_for_make(outname);

	if (operating_mode & OP_DEPEND) {
		char *line;

		if (depend_missing_ok)
			preproc->include_path(NULL);    /* "assume generated" */

		preproc->reset(inname, 0, &nasmlist, depend_ptr);
		if (outname[0] == '\0')
			ofmt->filename(inname, outname);
		ofile = NULL;
		while ((line = preproc->getline()))
			nasm_free(line);
		preproc->cleanup(0);
	}
	else if (operating_mode & OP_PREPROCESS) {
		char *line;
		char *file_name = NULL;
		int32_t prior_linnum = 0;
		int lineinc = 0;

		if (*outname) {
			ofile = fopen(outname, "w");
			if (!ofile)
				nasm_error(ERR_FATAL | ERR_NOFILE,
					"unable to open output file `%s'",
					outname);
		}
		else
			ofile = NULL;

		location.known = false;

		/* pass = 1; */
		preproc->reset(inname, 3, &nasmlist, depend_ptr);
		memcpy(warning_on, warning_on_global, (ERR_WARN_MAX + 1) * sizeof(bool));

		while ((line = preproc->getline())) {
			/*
			* We generate %line directives if needed for later programs
			*/
			int32_t linnum = prior_linnum += lineinc;
			int altline = src_get(&linnum, &file_name);
			if (altline) {
				if (altline == 1 && lineinc == 1)
					nasm_fputs("", ofile);
				else {
					lineinc = (altline != -1 || lineinc != 1);
					fprintf(ofile ? ofile : stdout,
						"%%line %"PRId32"+%d %s\n", linnum, lineinc,
						file_name);
				}
				prior_linnum = linnum;
			}
			nasm_fputs(line, ofile);
			nasm_free(line);
		}
		nasm_free(file_name);
		preproc->cleanup(0);
		if (ofile)
			fclose(ofile);
		if (ofile && terminate_after_phase)
			remove(outname);
		ofile = NULL;
	}

	if (operating_mode & OP_NORMAL) {
		/*
		* We must call ofmt->filename _anyway_, even if the user
		* has specified their own output file, because some
		* formats (eg OBJ and COFF) use ofmt->filename to find out
		* the name of the input file and then put that inside the
		* file.
		*/
		ofmt->filename(inname, outname);

		ofile = fopen(outname, (ofmt->flags & OFMT_TEXT) ? "w" : "wb");
		if (!ofile)
			nasm_error(ERR_FATAL | ERR_NOFILE,
				"unable to open output file `%s'", outname);

		/*
		* We must call init_labels() before ofmt->init() since
		* some object formats will want to define labels in their
		* init routines. (eg OS/2 defines the FLAT group)
		*/
		init_labels();

		ofmt->init();
		dfmt = ofmt->current_dfmt;
		dfmt->init();

		assemble_file(inname, depend_ptr);

		if (!terminate_after_phase) {
			ofmt->cleanup(using_debug_info);
			cleanup_labels();
			fflush(ofile);
			if (ferror(ofile))
				nasm_error(ERR_NONFATAL | ERR_NOFILE,
					"write error on output file `%s'", outname);
		}

		if (ofile) {
			fclose(ofile);
			if (terminate_after_phase)
				remove(outname);
			ofile = NULL;
		}
	}

	if (depend_list && !terminate_after_phase)
		emit_dependencies(depend_list);

	if (want_usage)
		usage();

	raa_free(offsets);
	saa_free(forwrefs);
	eval_cleanup();
	stdscan_cleanup();

	return terminate_after_phase;
}
