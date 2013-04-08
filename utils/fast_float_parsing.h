/* fast_float_parsing.h                                            -*- C++ -*-
   Jeremy Barnes, 25 February 2005
   Copyright (c) 2005 Jeremy Barnes.  All rights reserved.
   
   This file is part of "Jeremy's Machine Learning Library", copyright (c)
   1999-2005 Jeremy Barnes.
   
   This program is available under the GNU General Public License, the terms
   of which are given by the file "license.txt" in the top level directory of
   the source code distribution.  If this file is missing, you have no right
   to use the program; please contact the author.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   ---

   Fast inline float parsing routines.
*/

#ifndef __utils__fast_float_parsing_h__
#define __utils__fast_float_parsing_h__


#include "jml/utils/parse_context.h"
#include <limits>
#include <errno.h>

namespace ML {

double binary_exp10 [10] = {
    10,
    100,
    1e4,
    1e8,
    1e16,
    1e32,
    1e64,
    1e128,
    1e256,
    INFINITY
};

double binary_exp10_neg [10] = {
    0.1,
    0.01,
    1e-4,
    1e-8,
    1e-16,
    1e-32,
    1e-64,
    1e-128,
    1e-256,
    0.0
};

double
exp10_int(int val)
{
    double result = 1.0;

    if (val >= 0) {
        for (unsigned i = 0;  val;  ++i) {
            if (i >= 9)
                return INFINITY;
            if (val & 1)
                result *= binary_exp10[i];
            val >>= 1;
        }
    }
    else {
        val = -val;
        for (unsigned i = 0;  val;  ++i) {
            if (i >= 9)
                return 0.0;
            if (val & 1)
                result *= binary_exp10_neg[i];
            val >>= 1;
        }
    }

    return result;
}

template<typename Float>
inline bool match_float(Float & result, Parse_Context & c)
{
    Parse_Context::Revert_Token tok(c);

    unsigned long num = 0;
    double den = 0.0;
    unsigned long den2 = 0;
    int den2_digits = 0;
    double sign = 1;
    int digits = 0;

    if (c.match_literal('+')) ;
    else if (c.match_literal('-')) sign = -1.0;

    if (c.eof()) return false;

    if (*c == 'n' || *c == 'N') {
        ++c;
        if (!c.match_literal("an") && !c.match_literal("aN"))
            return false;
        tok.ignore();
        result = sign * std::numeric_limits<Float>::quiet_NaN();
        return true;
    }
    else if (*c == 'i') {
        ++c;
        if (!c.match_literal("nf"))
            return false;
        tok.ignore();
        result = sign * INFINITY;
        return true;
    }

    bool inOverflow = false;

    while (c) {
        //std::cerr << "got character " << *c << " num = " << num
        //          << " den = " << den << " den2 = " << den2
        //          << " sign = " << sign
        //          << " digits = " << digits << std::endl;

        if (isdigit(*c)) {
            int digit = *c - '0';
            if (digits > 18 /* num >= std::numeric_limits<unsigned long>::max() / 10*/) {
                if (!inOverflow) {
                    // Overflow...
                    //std::cerr << "*** overflow" << std::endl;
                    //if (digit >= 5)
                    //    ++num;
                    inOverflow = true;
                }
            }
            else {
                num = 10*num + digit;
                den *= 0.1;
                den2 *= 10;
                ++den2_digits;
            }
            ++digits;
        }
        else if (*c == '.') {
            if (den != 0.0) break;
            else {
                den = 1.0;
                den2 = 1;
                den2_digits = 0;
            }
        }
        else if (digits && (*c == 'e' || *c == 'E')) {
            Parse_Context::Revert_Token token(c);
            ++c;
            if (c.match_literal('+'));
            int expi;
            if (c.match_int(expi)) {
                sign *= exp10(expi);
                token.ignore();
            }
            break;
        }
        else break;
        ++c;
    }

    if (!digits) return false;

    if (digits > 3 && false) {
        // we need to parse using strtod since rounding bites us otherwise
        size_t ofs = c.get_offset();

        // Go back
        tok.apply();

        size_t ofs0 = c.get_offset();
        size_t nchars = ofs - ofs0;
        
        char buf[nchars + 1];

        for (unsigned i = 0;  i < nchars;  ++i)
            buf[i] = *c++;
        buf[nchars] = 0;

        char * endptr;
        double parsed = strtod(buf, &endptr);

        if (endptr != buf + nchars)
            throw Exception("wrong endptr");

        result = parsed;
        return true;
    }

    tok.ignore();  // we are returning true; ignore the token
 
    if (digits > 15 && false) {
        // HACK: check one ulp up and down to see which ends up the closest
        // when converted to a double and then back
        double base = num;

        union {
            double d;
            uint64_t i;
        } u;

        u.d = base;
        u.i -= 1;
        double lower = u.d;
        u.i += 2;
        double upper = u.d;

        auto iabs = [] (int64_t val)
            {
                return val < 0 ? -val : val;
            };

#if 0
        cerr << "num = " << num
             << " base = " << (uint64_t)base
             << " lower = " << (uint64_t)lower
             << " upper = " << (uint64_t)upper
             << endl;
#endif

        uint64_t error1 = iabs(num - (uint64_t)base);
        uint64_t error2 = iabs(num - (uint64_t)lower);
        uint64_t error3 = iabs(num - (uint64_t)upper);

        cerr << "base  = " << ML::format("%.16f", base) << endl;
        cerr << "lower = " << ML::format("%.16f", lower) << endl;
        cerr << "upper = " << ML::format("%.16f", upper) << endl;
        
        cerr << "error1 = " << error1 << " error2 = " << error2
             << " error3 = " << error3 << endl;

        double best = base;
        uint64_t error = error1;

        if (error2 < error) {
            best = lower;
            error = error2;
        }
            
        if (error3 < error) {
            best = upper;
            error = error3;
        }

        if (den == 0.0) result = sign * num;
        else result = sign * best / den2;

        return true;
    }

#if 0
    using namespace std;
    cerr << "num = " << num << endl;
    cerr << "num2 = " << (uint64_t)(double)num << endl;
    cerr << "inum = " << ML::format("%016llx", num) << endl;
    cerr << "fnum = " << ML::format("%.16f", (double)num) << endl;
    cerr << "fden = " << ML::format("%.16f", (double)den2) << endl;
    cerr << "fden = " << ML::format("%.16f", 1.0/den2) << endl;
#endif   

    if (den == 0.0) result = sign * num;
    else result = sign * (double)num /* * exp10_int(-den2_digits);*/ / den2;

    return true;
}

template<typename Float>
inline Float expect_float(Parse_Context & c,
                          const char * error = "expected real number")
{
    Float result;
    if (!match_float(result, c))
        c.exception(error);
    return result;
}


} // namespace ML


#endif /* __utils__fast_float_parsing_h__ */
