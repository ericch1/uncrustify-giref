/**
 * @file func_proto.h
 *
 * @author  Guy Maurel
 * @author  Ben Gardner
 * @license GPL v2+
 */

#ifndef ALIGN_FUNC_PROTO_H_INCLUDED
#define ALIGN_FUNC_PROTO_H_INCLUDED

#include <cstddef>

//! Aligns all function prototypes in the file.
void align_func_proto(size_t span);


class Chunk;

/**
 * Whether 'paren' is an open parenthesis aligned by align_func_proto_paren:
 * the one of a function prototype, constructor or destructor, or of a
 * function definition with align_single_line_func, but not of a delegating
 * constructor call nor of a function after another one on the same line.
 */
bool func_proto_paren_is_aligned(Chunk *paren);

#endif /* ALIGN_FUNC_PROTO_H_INCLUDED */
