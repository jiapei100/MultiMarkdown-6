/**

	MultiMarkdown 6 -- Lightweight markup processor to produce HTML, LaTeX, and more.

	@file html.c

	@brief Convert token tree to HTML output.


	@author	Fletcher T. Penney
	@bug	

**/

/*

	Copyright © 2016 - 2017 Fletcher T. Penney.


	The `MultiMarkdown 6` project is released under the MIT License..
	
	GLibFacade.c and GLibFacade.h are from the MultiMarkdown v4 project:
	
		https://github.com/fletcher/MultiMarkdown-4/
	
	MMD 4 is released under both the MIT License and GPL.
	
	
	CuTest is released under the zlib/libpng license. See CuTest.c for the text
	of the license.
	
	
	## The MIT License ##
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char.h"
#include "d_string.h"
#include "html.h"
#include "i18n.h"
#include "libMultiMarkdown.h"
#include "parser.h"
#include "token.h"
#include "scanners.h"
#include "writer.h"


#define print(x) d_string_append(out, x)
#define print_const(x) d_string_append_c_array(out, x, sizeof(x) - 1)
#define print_char(x) d_string_append_c(out, x)
#define printf(...) d_string_append_printf(out, __VA_ARGS__)
#define print_token(t) d_string_append_c_array(out, &(source[t->start]), t->len)
#define print_localized(x) mmd_print_localized_char_html(out, x, scratch)

// Use Knuth's pseudo random generator to obfuscate email addresses predictably
long ran_num_next();

void mmd_print_char_html(DString * out, char c, bool obfuscate) {
	switch (c) {
		case '"':
			print_const("&quot;");
			break;
		case '&':
			print_const("&amp;");
			break;
		case '<':
			print_const("&lt;");
			break;
		case '>':
			print_const("&gt;");
			break;
		default:
			if (obfuscate && ((int) c == (((int) c) & 127))) {
				if (ran_num_next() % 2 == 0)
					printf("&#%d;", (int) c);
				else
					printf("&#x%x;", (unsigned int) c);
			} else {
				print_char(c);
			}
			break;
	}
}


void mmd_print_string_html(DString * out, const char * str, bool obfuscate) {
	while (*str != '\0') {
		mmd_print_char_html(out, *str, obfuscate);
		str++;
	}
}


void mmd_print_localized_char_html(DString * out, unsigned short type, scratch_pad * scratch) {
	switch (type) {
		case DASH_N:
			print_const("&#8211;");
			break;
		case DASH_M:
			print_const("&#8212;");
			break;
		case ELLIPSIS:
			print_const("&#8230;");
			break;
		case APOSTROPHE:
			print_const("&#8217;");
			break;
		case QUOTE_LEFT_SINGLE:
			switch (scratch->quotes_lang) {
				case SWEDISH:
					print( "&#8217;");
					break;
				case FRENCH:
					print_const("&#39;");
					break;
				case GERMAN:
					print_const("&#8218;");
					break;
				case GERMANGUILL:
					print_const("&#8250;");
					break;
				default:
					print_const("&#8216;");
				}
			break;
		case QUOTE_RIGHT_SINGLE:
			switch (scratch->quotes_lang) {
				case GERMAN:
					print_const("&#8216;");
					break;
				case GERMANGUILL:
					print_const("&#8249;");
					break;
				default:
					print_const("&#8217;");
				}
			break;
		case QUOTE_LEFT_DOUBLE:
			switch (scratch->quotes_lang) {
				case DUTCH:
				case GERMAN:
					print_const("&#8222;");
					break;
				case GERMANGUILL:
					print_const("&#187;");
					break;
				case FRENCH:
					print_const("&#171;");
					break;
				case SWEDISH:
					print( "&#8221;");
					break;
				default:
					print_const("&#8220;");
				}
			break;
		case QUOTE_RIGHT_DOUBLE:
			switch (scratch->quotes_lang) {
				case GERMAN:
					print_const("&#8220;");
					break;
				case GERMANGUILL:
					print_const("&#171;");
					break;
				case FRENCH:
					print_const("&#187;");
					break;
				case SWEDISH:
				case DUTCH:
				default:
					print_const("&#8221;");
				}
			break;
	}
}


void mmd_export_link_html(DString * out, const char * source, token * text, link * link, size_t offset, scratch_pad * scratch) {
	attr * a = link->attributes;

	if (link->url) {
		print_const("<a href=\"");
		mmd_print_string_html(out, link->url, false);
		print_const("\"");
	} else
		print_const("<a href=\"\"");

	if (link->title && link->title[0] != '\0') {
		print_const(" title=\"");
		mmd_print_string_html(out, link->title, false);
		print_const("\"");
	}

	while (a) {
		print_const(" ");
		print(a->key);
		print_const("=\"");
		print(a->value);
		print_const("\"");
		a = a->next;
	}

	print_const(">");

	// If we're printing contents of bracket as text, then ensure we include it all
	if (text && text->child && text->child->len > 1) {
		text->child->next->start--;
		text->child->next->len++;
	}
	
	mmd_export_token_tree_html(out, source, text->child, offset, scratch);

	print_const("</a>");
}


void mmd_export_image_html(DString * out, const char * source, token * text, link * link, size_t offset, scratch_pad * scratch, bool is_figure) {
	attr * a = link->attributes;

	// Compatibility mode doesn't allow figures
	if (scratch->extensions & EXT_COMPATIBILITY)
		is_figure = false;

	if (is_figure) {
		// Remove wrapping <p> markers
		d_string_erase(out, out->currentStringLength - 3, 3);
		print_const("<figure>\n");
		scratch->close_para = false;
	}

	if (link->url)
		printf("<img src=\"%s\"", link->url);
	else
		print_const("<img src=\"\"");

	if (text) {
		print_const(" alt=\"");
		print_token_tree_raw(out, source, text->child);
		print_const("\"");
	}

	if (link->label && !(scratch->extensions & EXT_COMPATIBILITY)) {
		// \todo: Need to decide on approach to id's
		char * label = label_from_token(source, link->label);
		printf(" id=\"%s\"", label);
		free(label);
	}

	if (link->title && link->title[0] != '\0')
		printf(" title=\"%s\"", link->title);

	while (a) {
		print_const(" ");
		print(a->key);
		print_const("=\"");
		print(a->value);
		print_const("\"");
		a = a->next;
	}

	print_const(" />");

	if (is_figure) {
		if (text) {
			print_const("\n<figcaption>");
			mmd_export_token_tree_html(out, source, text->child, offset, scratch);
			print_const("</figcaption>");
		}
		print_const("\n</figure>");
	}
}


void mmd_export_token_html(DString * out, const char * source, token * t, size_t offset, scratch_pad * scratch) {
	if (t == NULL)
		return;

	short	temp_short;
	short	temp_short2;
	link *	temp_link	= NULL;
	char *	temp_char	= NULL;
	char *	temp_char2	= NULL;
	bool	temp_bool	= 0;
	token *	temp_token	= NULL;
	footnote * temp_note = NULL;

	switch (t->type) {
		case AMPERSAND:
		case AMPERSAND_LONG:
			print_const("&amp;");
			break;
		case ANGLE_LEFT:
			print_const("&lt;");
			break;
		case ANGLE_RIGHT:
			print_const("&gt;");
			break;
		case APOSTROPHE:
			if (!(scratch->extensions & EXT_SMART)) {
				print_token(t);
			} else {
				print_localized(APOSTROPHE);
			}
			break;
		case BACKTICK:
			if (t->mate == NULL)
				print_token(t);
			else if (t->mate->type == QUOTE_RIGHT_ALT)
				if (!(scratch->extensions & EXT_SMART)) {
					print_token(t);
				} else {
					print_localized(QUOTE_LEFT_DOUBLE);
				}
			else if (t->start < t->mate->start) {
				print_const("<code>");
			} else {
				print_const("</code>");
			}
			break;
		case BLOCK_BLOCKQUOTE:
			pad(out, 2, scratch);
			print_const("<blockquote>\n");
			scratch->padded = 2;
			mmd_export_token_tree_html(out, source, t->child, t->start + offset, scratch);
			pad(out, 1, scratch);
			print_const("</blockquote>");
			scratch->padded = 0;
			break;
		case BLOCK_DEFINITION:
			pad(out, 2, scratch);
			print_const("<dd>");

			temp_short = scratch->list_is_tight;
			if (!(t->child->next && (t->child->next->type == BLOCK_EMPTY) && t->child->next->next))
				scratch->list_is_tight = true;

			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			print_const("</dd>");
			scratch->padded = 0;

			scratch->list_is_tight = temp_short;
			break;
		case BLOCK_DEFLIST:
			pad(out, 2, scratch);

			// Group consecutive definition lists into a single list.
			// lemon's LALR(1) parser can't properly handle this (to my understanding).

			if (!(t->prev && (t->prev->type == BLOCK_DEFLIST)))
				print_const("<dl>\n");
	
			scratch->padded = 2;

			mmd_export_token_tree_html(out, source, t->child, t->start + offset, scratch);
			pad(out, 1, scratch);

			if (!(t->next && (t->next->type == BLOCK_DEFLIST)))
				print_const("</dl>\n");

			scratch->padded = 1;
			break;
		case BLOCK_CODE_FENCED:
			pad(out, 2, scratch);
			print_const("<pre><code");

			temp_char = get_fence_language_specifier(t->child->child, source);
			if (temp_char) {
				printf(" class=\"%s\"", temp_char);
				free(temp_char);
			}

			print_const(">");
			mmd_export_token_tree_html_raw(out, source, t->child->next, t->start + offset, scratch);
			print_const("</code></pre>");
			scratch->padded = 0;
			break;
		case BLOCK_CODE_INDENTED:
			pad(out, 2, scratch);
			print_const("<pre><code>");
			mmd_export_token_tree_html_raw(out, source, t->child, t->start + offset, scratch);
			print_const("</code></pre>");
			scratch->padded = 0;
			break;
		case BLOCK_EMPTY:
			break;
		case BLOCK_H1:
		case BLOCK_H2:
		case BLOCK_H3:
		case BLOCK_H4:
		case BLOCK_H5:
		case BLOCK_H6:
			pad(out, 2, scratch);
			temp_short = t->type - BLOCK_H1 + 1;
			if (scratch->extensions & EXT_NO_LABELS) {
				printf("<h%1d>", temp_short + scratch->base_header_level - 1);
			} else {
				temp_char = label_from_header(source, t);
				printf("<h%1d id=\"%s\">", temp_short + scratch->base_header_level - 1, temp_char);
				free(temp_char);
			}
			mmd_export_token_tree_html(out, source, t->child, t->start + offset, scratch);
			printf("</h%1d>", temp_short + scratch->base_header_level - 1);
			scratch->padded = 0;
			break;
		case BLOCK_HR:
			pad(out, 2, scratch);
			print_const("<hr />");
			scratch->padded = 0;
			break;
		case BLOCK_HTML:
			pad(out, 2, scratch);
			print_token_raw(out, source, t);
			scratch->padded = 1;
			break;
		case BLOCK_LIST_BULLETED_LOOSE:
		case BLOCK_LIST_BULLETED:
			temp_short = scratch->list_is_tight;
			switch (t->type) {
				case BLOCK_LIST_BULLETED_LOOSE:
					scratch->list_is_tight = false;
					break;
				case BLOCK_LIST_BULLETED:
					scratch->list_is_tight = true;
					break;
			}
			pad(out, 2, scratch);
			print_const("<ul>");
			scratch->padded = 0;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			pad(out, 1, scratch);
			print_const("</ul>");
			scratch->padded = 0;
			scratch->list_is_tight = temp_short;
			break;
		case BLOCK_LIST_ENUMERATED_LOOSE:
		case BLOCK_LIST_ENUMERATED:
			temp_short = scratch->list_is_tight;
			switch (t->type) {
				case BLOCK_LIST_ENUMERATED_LOOSE:
					scratch->list_is_tight = false;
					break;
				case BLOCK_LIST_ENUMERATED:
					scratch->list_is_tight = true;
					break;
			}
			pad(out, 2, scratch);
			print_const("<ol>");
			scratch->padded = 0;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			pad(out, 1, scratch);
			print_const("</ol>");
			scratch->padded = 0;
			scratch->list_is_tight = temp_short;
			break;
		case BLOCK_LIST_ITEM:
			pad(out, 1, scratch);
			print_const("<li>");
			scratch->padded = 2;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			print_const("</li>");
			scratch->padded = 0;
			break;
		case BLOCK_LIST_ITEM_TIGHT:
			pad(out, 1, scratch);
			print_const("<li>");

			if (!scratch->list_is_tight)
				print_const("<p>");

			scratch->padded = 2;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);

			if (scratch->close_para) {
				if (!scratch->list_is_tight)
					print_const("</p>");
			} else {
				scratch->close_para = true;
			}

			print_const("</li>");
			scratch->padded = 0;
			break;
		case BLOCK_META:
			break;
		case BLOCK_PARA:
		case BLOCK_DEF_CITATION:
		case BLOCK_DEF_FOOTNOTE:
		case BLOCK_DEF_GLOSSARY:
			pad(out, 2, scratch);
	
			if (!scratch->list_is_tight)
				print_const("<p>");

			mmd_export_token_tree_html(out, source, t->child, offset, scratch);

			if (scratch->citation_being_printed) {
				scratch->footnote_para_counter--;

				if (scratch->footnote_para_counter == 0) {
					printf(" <a href=\"#cnref:%d\" title=\"%s\" class=\"reversecitation\">&#160;&#8617;</a>", scratch->citation_being_printed, LC("return to body"));
				}
			}

			if (scratch->footnote_being_printed) {
				scratch->footnote_para_counter--;

				if (scratch->footnote_para_counter == 0) {
					printf(" <a href=\"#fnref:%d\" title=\"%s\" class=\"reversefootnote\">&#160;&#8617;</a>", scratch->footnote_being_printed, LC("return to body"));
				}
			}

			if (scratch->glossary_being_printed) {
				scratch->footnote_para_counter--;

				if (scratch->footnote_para_counter == 0) {
					printf(" <a href=\"#gnref:%d\" title=\"%s\" class=\"reverseglossary\">&#160;&#8617;</a>", scratch->glossary_being_printed, LC("return to body"));
				}
			}

			if (scratch->close_para) {
				if (!scratch->list_is_tight)
					print_const("</p>");
			} else {
				scratch->close_para = true;
			}
			scratch->padded = 0;
			break;
		case BLOCK_SETEXT_1:
			pad(out, 2, scratch);
			temp_short = 1;
			if (scratch->extensions & EXT_NO_LABELS) {
				printf("<h%1d>", temp_short + scratch->base_header_level - 1);
			} else {
				temp_token = manual_label_from_header(t, source);
				if (temp_token) {
					temp_char = label_from_token(source, temp_token);
				} else {
					temp_char = label_from_token(source, t);
				}
				printf("<h%1d id=\"%s\">", temp_short + scratch->base_header_level - 1, temp_char);
				free(temp_char);
			}
			mmd_export_token_tree_html(out, source, t->child, t->start + offset, scratch);
			printf("</h%1d>", temp_short + scratch->base_header_level - 1);
			scratch->padded = 0;
			break;
		case BLOCK_SETEXT_2:
			pad(out, 2, scratch);
			temp_short = 2;
			if (scratch->extensions & EXT_NO_LABELS) {
				printf("<h%1d>", temp_short + scratch->base_header_level - 1);
			} else {
				temp_token = manual_label_from_header(t, source);
				if (temp_token) {
					temp_char = label_from_token(source, temp_token);
				} else {
					temp_char = label_from_token(source, t);
				}
				printf("<h%1d id=\"%s\">", temp_short + scratch->base_header_level - 1, temp_char);
				free(temp_char);
			}
			mmd_export_token_tree_html(out, source, t->child, t->start + offset, scratch);
			printf("</h%1d>", temp_short + scratch->base_header_level - 1);
			scratch->padded = 0;
			break;
		case BLOCK_TABLE:
			pad(out, 2, scratch);
			print_const("<table>\n");

			// Are we followed by a caption?
			if (table_has_caption(t)) {
				temp_token = t->next->child;

				if (temp_token->next &&
					temp_token->next->type == PAIR_BRACKET) {
					temp_token = temp_token->next;
				}

				temp_char = label_from_token(source, temp_token);
				printf("<caption id=\"%s\">", temp_char);
				free(temp_char);

				t->next->child->child->type = TEXT_EMPTY;
				t->next->child->child->mate->type = TEXT_EMPTY;
				mmd_export_token_tree_html(out, source, t->next->child->child, offset, scratch);
				print_const("</caption>\n");
				temp_short = 1;
			} else {
				temp_short = 0;
			}

			scratch->padded = 2;
			read_table_column_alignments(source, t, scratch);

			print_const("<colgroup>\n");
			for (int i = 0; i < scratch->table_column_count; ++i)
			{
				switch (scratch->table_alignment[i]) {
					case 'l':
						print_const("<col style=\"text-align:left;\"/>\n");
						break;
					case 'L':
						print_const("<col style=\"text-align:left;\" class=\"extended\"/>\n");
						break;
					case 'r':
						print_const("<col style=\"text-align:right;\"/>\n");
						break;
					case 'R':
						print_const("<col style=\"text-align:right;\" class=\"extended\"/>\n");
						break;
					case 'c':
						print_const("<col style=\"text-align:center;\"/>\n");
						break;
					case 'C':
						print_const("<col style=\"text-align:center;\" class=\"extended\"/>\n");
						break;
					default:
						print_const("<col />\n");
						break;
				}
			}
			print_const("</colgroup>\n");
			scratch->padded = 1;

			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			pad(out, 1, scratch);
			print_const("</table>");
			scratch->padded = 0;

			scratch->skip_token = temp_short;

			break;
		case BLOCK_TABLE_HEADER:
			pad(out, 2, scratch);
			print_const("<thead>\n");
			scratch->in_table_header = 1;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			scratch->in_table_header = 0;
			print_const("</thead>\n");
			scratch->padded = 1;
			break;
		case BLOCK_TABLE_SECTION:
			pad(out, 2, scratch);
			print_const("<tbody>\n");
			scratch->padded = 2;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			print_const("</tbody>");
			scratch->padded = 0;
			break;
		case BLOCK_TERM:
			pad(out, 2, scratch);
			print_const("<dt>");
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			print_const("</dt>\n");
			scratch->padded = 2;
			break;
		case BLOCK_TOC:
			temp_short = 0;
			temp_short2 = 0;
			pad(out, 2, scratch);
			print_const("<div class=\"TOC\">");

			for (int i = 0; i < scratch->header_stack->size; ++i)
			{
				temp_token = stack_peek_index(scratch->header_stack, i);

				if (temp_token->type == temp_short2) {
					// Same level -- close list item
					print_const("</li>\n");
				}

				if (temp_short == 0) {
					// First item
					print_const("\n<ul>\n");
					temp_short = temp_token->type;
					temp_short2 = temp_short;
				}

				// Indent?
				if (temp_token->type == temp_short2) {
					// Same level -- NTD
				} else if (temp_token->type == temp_short2 + 1) {
					// Indent
					print_const("\n\n<ul>\n");
					temp_short2++;
				} else if (temp_token->type < temp_short2) {
					// Outdent
					print_const("</li>\n");
					while (temp_short2 > temp_token->type) {
						if (temp_short2 > temp_short)
							print_const("</ul></li>\n");
						else
							temp_short = temp_short2 - 1;

						temp_short2--;
					}
				} else {
					// Skipped more than one level -- ignore
					continue;
				}

				temp_char = label_from_header(source, temp_token);

				printf("<li><a href=\"#%s\">", temp_char);
				mmd_export_token_tree_html(out, source, temp_token->child, offset, scratch);
				print_const("</a>");
				free(temp_char);
			}

			while (temp_short2 > (temp_short)) {
				print_const("</ul>\n");
				temp_short2--;
			}
			
			if (temp_short)
				print_const("</li>\n</ul>\n");

			print_const("</div>");
			scratch->padded = 0;
			break;
		case BRACE_DOUBLE_LEFT:
			print_const("{{");
			break;
		case BRACE_DOUBLE_RIGHT:
			print_const("}}");
			break;
		case BRACKET_LEFT:
			print_const("[");			
			break;
		case BRACKET_CITATION_LEFT:
			print_const("[#");
			break;
		case BRACKET_FOOTNOTE_LEFT:
			print_const("[^");
			break;
		case BRACKET_GLOSSARY_LEFT:
			print_const("[?");
			break;
		case BRACKET_IMAGE_LEFT:
			print_const("![");
			break;
		case BRACKET_VARIABLE_LEFT:
			print_const("[\%");
			break;
		case BRACKET_RIGHT:
			print_const("]");
			break;
		case COLON:
			print_const(":");
			break;
		case CRITIC_ADD_OPEN:
			print_const("{++");
			break;
		case CRITIC_ADD_CLOSE:
			print_const("++}");
			break;
		case CRITIC_COM_OPEN:
			print_const("{&gt;&gt;");
			break;
		case CRITIC_COM_CLOSE:
			print_const("&lt;&lt;}");
			break;
		case CRITIC_DEL_OPEN:
			print_const("{--");
			break;
		case CRITIC_DEL_CLOSE:
			print_const("--}");
			break;
		case CRITIC_HI_OPEN:
			print_const("{==");
			break;
		case CRITIC_HI_CLOSE:
			print_const("==}");
			break;
		case CRITIC_SUB_OPEN:
			print_const("{~~");
			break;
		case CRITIC_SUB_DIV:
			print_const("~&gt;");
			break;
		case CRITIC_SUB_CLOSE:
			print_const("~~}");
			break;
		case DASH_M:
			if (!(scratch->extensions & EXT_SMART)) {
				print_token(t);
			} else {
				print_localized(DASH_M);
			}
			break;
		case DASH_N:
			if (!(scratch->extensions & EXT_SMART)) {
				print_token(t);
			} else {
				print_localized(DASH_N);
			}
			break;
		case DOC_START_TOKEN:
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			break;
		case ELLIPSIS:
			if (!(scratch->extensions & EXT_SMART)) {
				print_token(t);
			} else {
				print_localized(ELLIPSIS);
			}
			break;
		case EMPH_START:
			print_const("<em>");
			break;
		case EMPH_STOP:
			print_const("</em>");
			break;
		case EQUAL:
			print_const("=");
			break;
		case ESCAPED_CHARACTER:
			if (!(scratch->extensions & EXT_COMPATIBILITY) &&
				(source[t->start + 1] == ' ')) {
				print_const("&nbsp;");
			} else {
				mmd_print_char_html(out, source[t->start + 1], false);
			}
			break;
		case HASH1:
		case HASH2:
		case HASH3:
		case HASH4:
		case HASH5:
		case HASH6:
			print_token(t);
			break;
		case INDENT_SPACE:
			print_char(' ');
			break;
		case INDENT_TAB:
			print_char('\t');
			break;
		case LINE_LIST_BULLETED:
		case LINE_LIST_ENUMERATED:
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			break;
		case MARKER_BLOCKQUOTE:
		case MARKER_H1:
		case MARKER_H2:
		case MARKER_H3:
		case MARKER_H4:
		case MARKER_H5:
		case MARKER_H6:
			break;
		case MARKER_LIST_BULLET:
		case MARKER_LIST_ENUMERATOR:
			break;
		case MATH_BRACKET_OPEN:
			if (t->mate) {
				print_const("<span class=\"math\">\\[");
			} else
				print_const("\\[");
			break;
		case MATH_BRACKET_CLOSE:
			if (t->mate) {
				print_const("\\]</span>");
			} else
				print_const("\\]");
			break;
		case MATH_DOLLAR_SINGLE:
			if (t->mate) {
				(t->start < t->mate->start) ? ( print_const("<span class=\"math\">\\(") ) : ( print_const("\\)</span>") );
			} else {
				print_const("$");
			}
			break;
		case MATH_DOLLAR_DOUBLE:
			if (t->mate) {
				(t->start < t->mate->start) ? ( print_const("<span class=\"math\">\\[") ) : ( print_const("\\]</span>") );
			} else {
				print_const("$$");
			}
			break;
		case MATH_PAREN_OPEN:
			if (t->mate) {
				print_const("<span class=\"math\">\\(");
			} else
				print_const("\\(");
			break;
		case MATH_PAREN_CLOSE:
			if (t->mate) {
				print_const("\\)</span>");
			} else
				print_const("\\)");
			break;
		case NON_INDENT_SPACE:
			print_char(' ');
			break;
		case PAIR_BACKTICK:
			// Strip leading whitespace
			switch (t->child->next->type) {
				case TEXT_NL:
				case INDENT_TAB:
				case INDENT_SPACE:
				case NON_INDENT_SPACE:
					t->child->next->type = TEXT_EMPTY;
					break;
				case TEXT_PLAIN:
					while (t->child->next->len && char_is_whitespace(source[t->child->next->start])) {
						t->child->next->start++;
						t->child->next->len--;
					}
					break;
			}

			// Strip trailing whitespace
			switch (t->child->mate->prev->type) {
				case TEXT_NL:
				case INDENT_TAB:
				case INDENT_SPACE:
				case NON_INDENT_SPACE:
					t->child->mate->prev->type = TEXT_EMPTY;
					break;
				case TEXT_PLAIN:
					while (t->child->mate->prev->len && char_is_whitespace(source[t->child->mate->prev->start + t->child->mate->prev->len - 1])) {
						t->child->mate->prev->len--;
					}
					break;
			}
			t->child->type = TEXT_EMPTY;
			t->child->mate->type = TEXT_EMPTY;
			print_const("<code>");
			mmd_export_token_tree_html_raw(out, source, t->child, offset, scratch);
			print_const("</code>");
			break;
		case PAIR_ANGLE:
			temp_char = url_accept(source, t->start + 1, t->len - 2, NULL, true);

			if (temp_char) {
				if (scan_email(temp_char))
					temp_bool = true;
				else
					temp_bool = false;
				print_const("<a href=\"");
				mmd_print_string_html(out, temp_char, temp_bool);
				print_const("\">");
				mmd_print_string_html(out, temp_char, temp_bool);
				print_const("</a>");
			} else if (scan_html(&source[t->start])) {
				print_token(t);
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}

			free(temp_char);
			break;
		case PAIR_BRACES:
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			break;
		case PAIR_BRACKET:
			if ((scratch->extensions & EXT_NOTES) &&
				(t->next && t->next->type == PAIR_BRACKET_CITATION)) {
				goto parse_citation;
			}

		case PAIR_BRACKET_IMAGE:
			parse_brackets(source, scratch, t, &temp_link, &temp_short, &temp_bool);

			if (temp_link) {
				if (t->type == PAIR_BRACKET) {
					// Link
					mmd_export_link_html(out, source, t, temp_link, offset, scratch);
				} else {
					// Image -- should it be a figure (e.g. image is only thing in paragraph)?
					temp_token = t->next;

					if (temp_token &&
						((temp_token->type == PAIR_BRACKET) ||
						(temp_token->type == PAIR_PAREN))) {
						temp_token = temp_token->next;
					}

					if (temp_token && temp_token->type == TEXT_NL)
						temp_token = temp_token->next;

					if (temp_token && temp_token->type == TEXT_LINEBREAK)
						temp_token = temp_token->next;

					if (t->prev || temp_token) {
						mmd_export_image_html(out, source, t, temp_link, offset, scratch, false);
					} else {
						mmd_export_image_html(out, source, t, temp_link, offset, scratch, true);
					}
				}
				
				if (temp_bool) {
					link_free(temp_link);
				}

				scratch->skip_token = temp_short;

				return;
			}

			// No links exist, so treat as normal
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			break;
		case PAIR_BRACKET_CITATION:
			parse_citation:
			temp_bool = true;

			if (t->type == PAIR_BRACKET) {
				// This is a locator for subsequent citation
				temp_char = text_inside_pair(source, t);
				temp_char2 = label_from_string(temp_char);

				if (strcmp(temp_char2, "notcited") == 0) {
					free(temp_char2);
					free(temp_char);
					temp_char = strdup("");
					temp_bool = false;
				}

				if (temp_char[0] == '\0')
					temp_char2 = strdup("");
				else
					temp_char2 = strdup(", ");


				// Process the actual citation
				t = t->next;
			} else {
				temp_char = strdup("");
				temp_char2 = strdup("");
			}

			if (scratch->extensions & EXT_NOTES) {
				temp_short2 = scratch->used_citations->size;

				citation_from_bracket(source, scratch, t, &temp_short);

				if (temp_bool) {
					if (temp_short2 == scratch->used_citations->size) {
						// Repeat of earlier citation
						printf("<a href=\"#cn:%d\" title=\"%s\" class=\"citation\">[%s%s%d]</a>",
								temp_short, LC("see citation"), temp_char, temp_char2, temp_short);
					} else {
						// New citation
						printf("<a href=\"#cn:%d\" id=\"cnref:%d\" title=\"%s\" class=\"citation\">[%s%s%d]</a>",
								temp_short, temp_short, LC("see citation"), temp_char, temp_char2, temp_short);
					}
				}

				if (t->prev && (t->prev->type == PAIR_BRACKET)) {
					// Skip citation on next pass
					scratch->skip_token = 1;
				}
			} else {
				// Footnotes disabled
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}

			free(temp_char);
			free(temp_char2);
			break;
		case PAIR_BRACKET_FOOTNOTE:
			if (scratch->extensions & EXT_NOTES) {
				footnote_from_bracket(source, scratch, t, &temp_short);

				if (temp_short < scratch->used_footnotes->size) {
					// Re-using previous footnote
					printf("<a href=\"#fn:%d\" title=\"%s\" class=\"footnote\">[%d]</a>",
						   temp_short, LC("see footnote"), temp_short);
				} else {
					// This is a new footnote
					printf("<a href=\"#fn:%d\" id=\"fnref:%d\" title=\"%s\" class=\"footnote\">[%d]</a>",
						   temp_short, temp_short, LC("see footnote"), temp_short);
				}
			} else {
				// Footnotes disabled
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}
			break;
		case PAIR_BRACKET_GLOSSARY:
			if (scratch->extensions & EXT_NOTES) {
				glossary_from_bracket(source, scratch, t, &temp_short);

				if (temp_short == -1) {
					print_const("[?");
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
					print_const("]");
					break;
				}

				temp_note = stack_peek_index(scratch->used_glossaries, temp_short - 1);

				if (temp_short < scratch->used_glossaries->size) {
					// Re-using previous glossary
					printf("<a href=\"#gn:%d\" title=\"%s\" class=\"glossary\">",
						   temp_short, LC("see glossary"));

					mmd_print_string_html(out, temp_note->clean_text, false);

					print_const("</a>");
				} else {
					// This is a new glossary
					printf("<a href=\"#gn:%d\" id=\"gnref:%d\" title=\"%s\" class=\"glossary\">",
						   temp_short, temp_short, LC("see glossary"));

					mmd_print_string_html(out, temp_note->clean_text, false);

					print_const("</a>");
				}
			} else {
				// Footnotes disabled
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}
			break;
		case PAIR_BRACKET_VARIABLE:
			temp_char = text_inside_pair(source, t);
			temp_char2 = extract_metadata(scratch, temp_char);

			if (temp_char2)
				mmd_print_string_html(out, temp_char2, false);
			else
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);

			// Don't free temp_char2 (it belongs to meta *)
			free(temp_char);
			break;
		case PAIR_CRITIC_ADD:
			// Ignore if we're rejecting
			if (scratch->extensions & EXT_CRITIC_REJECT)
				break;
			if (scratch->extensions & EXT_CRITIC) {
				t->child->type = TEXT_EMPTY;
				t->child->mate->type = TEXT_EMPTY;
				if (scratch->extensions & EXT_CRITIC_ACCEPT) {
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
				} else {
					print_const("<ins>");
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
					print_const("</ins>");
				}
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);				
			}
			break;
		case PAIR_CRITIC_DEL:
			// Ignore if we're accepting
			if (scratch->extensions & EXT_CRITIC_ACCEPT)
				break;
			if (scratch->extensions & EXT_CRITIC) {
				t->child->type = TEXT_EMPTY;
				t->child->mate->type = TEXT_EMPTY;
				if (scratch->extensions & EXT_CRITIC_REJECT) {
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
				} else {
					print_const("<del>");
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
					print_const("</del>");
				}
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);				
			}
			break;
		case PAIR_CRITIC_COM:
			// Ignore if we're rejecting or accepting
			if ((scratch->extensions & EXT_CRITIC_REJECT) ||
				(scratch->extensions & EXT_CRITIC_ACCEPT))
				break;
			if (scratch->extensions & EXT_CRITIC) {
				t->child->type = TEXT_EMPTY;
				t->child->mate->type = TEXT_EMPTY;
				print_const("<span class=\"critic comment\">");
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
				print_const("</span>");
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}
			break;
		case PAIR_CRITIC_HI:
			// Ignore if we're rejecting or accepting
			if ((scratch->extensions & EXT_CRITIC_REJECT) ||
				(scratch->extensions & EXT_CRITIC_ACCEPT))
				break;
			if (scratch->extensions & EXT_CRITIC) {
				t->child->type = TEXT_EMPTY;
				t->child->mate->type = TEXT_EMPTY;
				print_const("<mark>");
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
				print_const("</mark>");
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}
			break;
		case CRITIC_SUB_DIV_A:
			print_const("~");
			break;
		case CRITIC_SUB_DIV_B:
			print_const("&gt;");
			break;
		case PAIR_CRITIC_SUB_DEL:
			if ((scratch->extensions & EXT_CRITIC) &&
				(t->next->type == PAIR_CRITIC_SUB_ADD)) {
				t->child->type = TEXT_EMPTY;
				t->child->mate->type = TEXT_EMPTY;
				if (scratch->extensions & EXT_CRITIC_ACCEPT) {

				} else if (scratch->extensions & EXT_CRITIC_REJECT) {
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
				} else {
					print_const("<del>");
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
					print_const("</del>");
				}
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}
			break;
		case PAIR_CRITIC_SUB_ADD:
			if ((scratch->extensions & EXT_CRITIC) &&
				(t->prev->type == PAIR_CRITIC_SUB_DEL)) {
				t->child->type = TEXT_EMPTY;
				t->child->mate->type = TEXT_EMPTY;
				if (scratch->extensions & EXT_CRITIC_REJECT) {

				} else if (scratch->extensions & EXT_CRITIC_ACCEPT) {
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
				} else {
					print_const("<ins>");
					mmd_export_token_tree_html(out, source, t->child, offset, scratch);
					print_const("</ins>");
				}
			} else {
				mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			}
			break;
		case PAIR_MATH:
		case PAIR_PAREN:
		case PAIR_QUOTE_DOUBLE:
		case PAIR_QUOTE_SINGLE:
		case PAIR_STAR:
		case PAIR_UL:
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			break;
		case PAREN_LEFT:
			print_const("(");
			break;
		case PAREN_RIGHT:
			print_const(")");
			break;
		case PIPE:
			print_token(t);
			break;
		case PLUS:
			print_token(t);
			break;
		case QUOTE_SINGLE:
			if ((t->mate == NULL) || (!(scratch->extensions & EXT_SMART)))
				print_const("'");
			else
				(t->start < t->mate->start) ? ( print_localized(QUOTE_LEFT_SINGLE) ) : ( print_localized(QUOTE_RIGHT_SINGLE) );
			break;
		case QUOTE_DOUBLE:
			if ((t->mate == NULL) || (!(scratch->extensions & EXT_SMART)))
				print_const("&quot;");
			else
				(t->start < t->mate->start) ? ( print_localized(QUOTE_LEFT_DOUBLE) ) : ( print_localized(QUOTE_RIGHT_DOUBLE) );
			break;
		case QUOTE_RIGHT_ALT:
			if ((t->mate == NULL) || (!(scratch->extensions & EXT_SMART)))
				print_const("''");
			else
				print_localized(QUOTE_RIGHT_DOUBLE);
			break;
		case SLASH:
		case STAR:
			print_token(t);
			break;
		case STRONG_START:
			print_const("<strong>");
			break;
		case STRONG_STOP:
			print_const("</strong>");
			break;
		case SUBSCRIPT:
			if (t->mate) {
				(t->start < t->mate->start) ? (print_const("<sub>")) : (print_const("</sub>"));
			} else if (t->len != 1) {
				print_const("<sub>");
				mmd_export_token_html(out, source, t->child, offset, scratch);
				print_const("</sub>");
			} else {
				print_const("~");
			}
			break;
		case SUPERSCRIPT:
			if (t->mate) {
				(t->start < t->mate->start) ? (print_const("<sup>")) : (print_const("</sup>"));
			} else if (t->len != 1) {
				print_const("<sup>");
				mmd_export_token_html(out, source, t->child, offset, scratch);
				print_const("</sup>");
			} else {
				print_const("^");
			}	
			break;
		case TABLE_CELL:
			if (scratch->in_table_header) {
				print_const("\t<th");
			} else {
				print_const("\t<td");
			}
			switch (scratch->table_alignment[scratch->table_cell_count]) {
				case 'l':
				case 'L':
					print_const(" style=\"text-align:left;\"");
					break;
				case 'r':
				case 'R':
					print_const(" style=\"text-align:right;\"");
					break;
				case 'c':
				case 'C':
					print_const(" style=\"text-align:center;\"");
					break;
			}
			if (t->next && t->next->type == TABLE_DIVIDER) {
				if (t->next->len > 1) {
					printf(" colspan=\"%d\"", t->next->len);
				}
			}
			print_const(">");
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			if (scratch->in_table_header) {
				print_const("</th>\n");
			} else {
				print_const("</td>\n");
			}
			if (t->next)
				scratch->table_cell_count += t->next->len;
			else
				scratch->table_cell_count++;
			
			break;
		case TABLE_DIVIDER:
			break;
		case TABLE_ROW:
			print_const("<tr>\n");
			scratch->table_cell_count = 0;
			mmd_export_token_tree_html(out, source, t->child, offset, scratch);
			print_const("</tr>\n");
			break;
		case TEXT_LINEBREAK:
			if (t->next) {
				print_const("<br />\n");
				scratch->padded = 1;
			}
			break;
		case CODE_FENCE:
		case TEXT_EMPTY:
		case MANUAL_LABEL:
			break;
		case TEXT_NL:
			if (t->next)
				print_char('\n');
			break;
		case TEXT_BACKSLASH:
		case TEXT_BRACE_LEFT:
		case TEXT_BRACE_RIGHT:
		case TEXT_HASH:
		case TEXT_NUMBER_POSS_LIST:
		case TEXT_PERCENT:
		case TEXT_PERIOD:
		case TEXT_PLAIN:
		case TOC:
			print_token(t);
			break;
		case UL:
			print_token(t);
			break;
		default:
			fprintf(stderr, "Unknown token type: %d\n", t->type);
			token_describe(t, source);
			break;
	}
}


void mmd_export_token_tree_html(DString * out, const char * source, token * t, size_t offset, scratch_pad * scratch) {

	// Prevent stack overflow with "dangerous" input causing extreme recursion
	if (scratch->recurse_depth == kMaxExportRecursiveDepth) {
		return;
	}

	scratch->recurse_depth++;

	while (t != NULL) {
		if (scratch->skip_token) {
			scratch->skip_token--;
		} else {
			mmd_export_token_html(out, source, t, offset, scratch);
		}

		t = t->next;
	}

	scratch->recurse_depth--;
}


void mmd_export_token_html_raw(DString * out, const char * source, token * t, size_t offset, scratch_pad * scratch) {
	if (t == NULL)
		return;

	switch (t->type) {
		case BACKTICK:
			print_token(t);
			break;
		case AMPERSAND:
			print_const("&amp;");
			break;
		case AMPERSAND_LONG:
			print_const("&amp;amp;");
			break;
		case ANGLE_RIGHT:
			print_const("&gt;");
			break;
		case ANGLE_LEFT:
			print_const("&lt;");
			break;
		case ESCAPED_CHARACTER:
			print_const("\\");
			mmd_print_char_html(out, source[t->start + 1], false);
			break;
		case QUOTE_DOUBLE:
			print_const("&quot;");
			break;
		case CODE_FENCE:
			if (t->next)
				t->next->type = TEXT_EMPTY;
		case TEXT_EMPTY:
			break;
		default:
			if (t->child)
				mmd_export_token_tree_html_raw(out, source, t->child, offset, scratch);
			else
				print_token(t);
			break;
	}
}


void mmd_start_complete_html(DString * out, const char * source, scratch_pad * scratch) {
	print_const("<!DOCTYPE html>\n<html>\n<head>\n\t<meta charset=\"utf-8\"/>\n");

	// Iterate over metadata keys
	meta * m;

	for (m = scratch->meta_hash; m != NULL; m = m->hh.next) {
		if (strcmp(m->key, "baseheaderlevel") == 0) {
		} else if (strcmp(m->key, "bibtex") == 0) {
		} else if (strcmp(m->key, "css") == 0) {
			print_const("\t<link type=\"text/css\" rel=\"stylesheet\" href=\"");
			mmd_print_string_html(out, m->value, false);
			print_const("\"/>\n");
		} else if (strcmp(m->key, "htmlfooter") == 0) {
		} else if (strcmp(m->key, "htmlheader") == 0) {
			print(m->value);
			print_char('\n');
		} else if (strcmp(m->key, "htmlheaderlevel") == 0) {
		} else if (strcmp(m->key, "lang") == 0) {
		} else if (strcmp(m->key, "latexbegin") == 0) {
		} else if (strcmp(m->key, "latexconfig") == 0) {
		} else if (strcmp(m->key, "latexfooter") == 0) {
		} else if (strcmp(m->key, "latexheaderlevel") == 0) {
		} else if (strcmp(m->key, "latexinput") == 0) {
		} else if (strcmp(m->key, "latexleader") == 0) {
		} else if (strcmp(m->key, "latexmode") == 0) {
		} else if (strcmp(m->key, "mmdfooter") == 0) {
		} else if (strcmp(m->key, "mmdheader") == 0) {
		} else if (strcmp(m->key, "quoteslanguage") == 0) {
		} else if (strcmp(m->key, "title") == 0) {
			print_const("\t<title>");
			mmd_print_string_html(out, m->value, false);
			print_const("</title>\n");
		} else if (strcmp(m->key, "transcludebase") == 0) {
		} else if (strcmp(m->key, "xhtmlheader") == 0) {
			print(m->value);
			print_char('\n');
		} else if (strcmp(m->key, "xhtmlheaderlevel") == 0) {
		} else {
			print_const("\t<meta name=\"");
			mmd_print_string_html(out, m->key, false);
			print_const("\" content=\"");
			mmd_print_string_html(out, m->value, false);
			print_const("\"/>\n");
		}
	}

	print_const("</head>\n<body>\n\n");
}


void mmd_end_complete_html(DString * out, const char * source, scratch_pad * scratch) {
	print_const("\n\n</body>\n</html>\n");
}


void mmd_export_token_tree_html_raw(DString * out, const char * source, token * t, size_t offset, scratch_pad * scratch) {
	while (t != NULL) {
		if (scratch->skip_token) {
			scratch->skip_token--;
		} else {
			mmd_export_token_html_raw(out, source, t, offset, scratch);
		}

		t = t->next;
	}
}


void mmd_export_footnote_list_html(DString * out, const char * source, scratch_pad * scratch) {
	if (scratch->used_footnotes->size > 0) {
		footnote * note;
		token * content;

		pad(out, 2, scratch);
		print_const("<div class=\"footnotes\">\n<hr />\n<ol>");
		scratch->padded = 0;

		for (int i = 0; i < scratch->used_footnotes->size; ++i)
		{
			// Export footnote
			pad(out, 2, scratch);

			printf("<li id=\"fn:%d\">\n", i + 1);
			scratch->padded = 6;

			note = stack_peek_index(scratch->used_footnotes, i);
			content = note->content;

			scratch->footnote_para_counter = 0;

			// We need to know which block is the last one in the footnote
			while(content) {
				if (content->type == BLOCK_PARA)
					scratch->footnote_para_counter++;
				
				content = content->next;
			}

			content = note->content;
			scratch->footnote_being_printed = i + 1;

			mmd_export_token_tree_html(out, source, content, 0, scratch);

			pad(out, 1, scratch);
			printf("</li>");
			scratch->padded = 0;
		}

		pad(out, 2, scratch);
		print_const("</ol>\n</div>");
		scratch->padded = 0;
		scratch->footnote_being_printed = 0;
	}
}


void mmd_export_glossary_list_html(DString * out, const char * source, scratch_pad * scratch) {
	if (scratch->used_glossaries->size > 0) {
		footnote * note;
		token * content;

		pad(out, 2, scratch);
		print_const("<div class=\"glossary\">\n<hr />\n<ol>");
		scratch->padded = 0;

		for (int i = 0; i < scratch->used_glossaries->size; ++i)
		{
			// Export glossary
			pad(out, 2, scratch);

			printf("<li id=\"gn:%d\">\n", i + 1);
			scratch->padded = 6;

			note = stack_peek_index(scratch->used_glossaries, i);
			content = note->content;

			// Print term
			mmd_print_string_html(out, note->clean_text, false);
			print_const(": ");

			// Print contents
			scratch->footnote_para_counter = 0;

			// We need to know which block is the last one in the footnote
			while(content) {
				if (content->type == BLOCK_PARA)
					scratch->footnote_para_counter++;
				
				content = content->next;
			}

			content = note->content;
			scratch->glossary_being_printed = i + 1;

			mmd_export_token_tree_html(out, source, content, 0, scratch);

			pad(out, 1, scratch);
			printf("</li>");
			scratch->padded = 0;
		}

		pad(out, 2, scratch);
		print_const("</ol>\n</div>");
		scratch->padded = 0;
		scratch->glossary_being_printed = 0;
	}
}


void mmd_export_citation_list_html(DString * out, const char * source, scratch_pad * scratch) {
	if (scratch->used_citations->size > 0) {
		footnote * note;
		token * content;

		pad(out, 2, scratch);
		print_const("<div class=\"citations\">\n<hr />\n<ol>");
		scratch->padded = 0;

		for (int i = 0; i < scratch->used_citations->size; ++i)
		{
			// Export footnote
			pad(out, 2, scratch);

			printf("<li id=\"cn:%d\">\n", i + 1);
			scratch->padded = 6;

			note = stack_peek_index(scratch->used_citations, i);
			content = note->content;

			scratch->footnote_para_counter = 0;

			// We need to know which block is the last one in the footnote
			while(content) {
				if (content->type == BLOCK_PARA)
					scratch->footnote_para_counter++;
				
				content = content->next;
			}

			content = note->content;
			scratch->citation_being_printed = i + 1;

			mmd_export_token_tree_html(out, source, content, 0, scratch);

			pad(out, 1, scratch);
			printf("</li>");
			scratch->padded = 0;
		}

		pad(out, 2, scratch);
		print_const("</ol>\n</div>");
		scratch->padded = 0;
		scratch->citation_being_printed = 0;
	}
}


