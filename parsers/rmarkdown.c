/*
 *
 *  Copyright (c) 2022, Masatake YAMATO
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 * This module contains functions for generating tags for R Markdown files.
 * https://bookdown.org/yihui/rmarkdown/
 *
 */

/*
 *   INCLUDE FILES
 */
#include "general.h"	/* must always come first */
#include "x-markdown.h"

#include "entry.h"
#include "parse.h"
#include "read.h"

#include <ctype.h>
#include <string.h>

/*
 *   DATA DEFINITIONS
 */
typedef enum {
	K_CHUNK_LABEL = 0,
} rmarkdownKind;

static kindDefinition RMarkdownKinds[] = {
	{ true, 'l', "chunklabel",       "chunk labels"},
};

struct sRMarkdownSubparser {
	markdownSubparser markdown;
	int lastChunkLabel;
};

/*
*   FUNCTION DEFINITIONS
*/

static void findRMarkdownTags (void)
{
	scheduleRunningBaseparser (0);
}

#define skip_space(CP) 	while (*CP == ' ' || *CP == '\t') CP++;

static int makeRMarkdownTag (vString *name, int kindIndex, bool anonymous)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue (name), kindIndex);
	if (anonymous)
		markTagExtraBit (&e, XTAG_ANONYMOUS);
	return makeTagEntry (&e);
}

static bool extractLanguageForCodeBlock (markdownSubparser *s,
										 const char *langMarker,
										 vString *langName)
{
	struct sRMarkdownSubparser *rmarkdown = (struct sRMarkdownSubparser *)s;
	const char *cp = langMarker;

	if (*cp != '{')
		return false;
	cp++;

	const char *end = strpbrk(cp, " \t,}");
	if (!end)
		return false;

	if (end - cp == 0)
		return false;

	vStringNCatS (langName, cp, end - cp);

	cp = end;
	if (*cp == ',' || *cp == '}')
	{
		vString *name = anonGenerateNew("__anon", K_CHUNK_LABEL);
		rmarkdown->lastChunkLabel = makeRMarkdownTag (name, K_CHUNK_LABEL, true);
		vStringDelete (name);
		return true;
	}

	skip_space(cp);

	vString *chunk_label  = vStringNew ();
	bool anonymous = false;
	while (isalnum((unsigned char)*cp) || *cp == '-')
		vStringPut (chunk_label, *cp++);

	if (vStringLength (chunk_label) == 0)
	{
		anonGenerate (chunk_label, "__anon", K_CHUNK_LABEL);
		anonymous = true;
	}

	skip_space(cp);
	if (*cp == ',' || *cp == '}')
		rmarkdown->lastChunkLabel = makeRMarkdownTag (chunk_label, K_CHUNK_LABEL, anonymous);

	vStringDelete (chunk_label);
	return true;
}

static void notifyEndOfCodeBlock (markdownSubparser *s)
{
	struct sRMarkdownSubparser *rmarkdown = (struct sRMarkdownSubparser *)s;

	if (rmarkdown->lastChunkLabel == CORK_NIL)
		return;

	setTagEndLineToCorkEntry (rmarkdown->lastChunkLabel, getInputLineNumber ());

	rmarkdown->lastChunkLabel = CORK_NIL;
}

static void inputStart (subparser *s)
{
	struct sRMarkdownSubparser *rmarkdown = (struct sRMarkdownSubparser*)s;

	rmarkdown->lastChunkLabel = CORK_NIL;
}

extern parserDefinition* RMarkdownParser (void)
{
	static const char *const extensions [] = { "rmd", NULL };
	static struct sRMarkdownSubparser rmarkdownSubparser = {
		.markdown = {
			.subparser = {
				.direction = SUBPARSER_SUB_RUNS_BASE,
				.inputStart = inputStart,
			},
			.extractLanguageForCodeBlock = extractLanguageForCodeBlock,
			.notifyEndOfCodeBlock = notifyEndOfCodeBlock,
		},
	};
	static parserDependency dependencies [] = {
		[0] = { DEPTYPE_SUBPARSER, "Markdown", &rmarkdownSubparser },
	};

	parserDefinition* const def = parserNew ("RMarkdown");


	def->dependencies = dependencies;
	def->dependencyCount = ARRAY_SIZE(dependencies);
	def->kindTable      = RMarkdownKinds;
	def->kindCount  = ARRAY_SIZE (RMarkdownKinds);
	def->extensions = extensions;
	def->parser     = findRMarkdownTags;
	return def;
}
