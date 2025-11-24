#!/usr/bin/env python3
"""
Numerical file comparison with tolerance for integration tests.
Compares two files line by line, allowing small floating-point differences.
"""

import sys
import re

def is_numeric(s):
    """Check if a string represents a number."""
    try:
        float(s)
        return True
    except ValueError:
        return False

def compare_values(val1_str, val2_str, tol):
    """Compare two numerical values with tolerance."""
    try:
        val1 = float(val1_str)
        val2 = float(val2_str)
        diff = abs(val1 - val2)
        
        # Use relative error if values are not too small
        if abs(val1) > 1e-10 and abs(val2) > 1e-10:
            rel_err = diff / max(abs(val1), abs(val2))
            return rel_err <= tol
        else:
            # Use absolute error for small values
            return diff <= tol
    except ValueError:
        return False

def compare_lines(line1, line2, tol):
    """Compare two lines with numerical tolerance."""
    if line1 == line2:
        return True
    
    # Split by whitespace (both space and tab)
    fields1 = re.split(r'[ \t]+', line1.strip())
    fields2 = re.split(r'[ \t]+', line2.strip())
    
    # Remove empty fields
    fields1 = [f for f in fields1 if f]
    fields2 = [f for f in fields2 if f]
    
    if len(fields1) != len(fields2):
        return False
    
    for f1, f2 in zip(fields1, fields2):
        if is_numeric(f1) and is_numeric(f2):
            if not compare_values(f1, f2, tol):
                return False
        else:
            if f1 != f2:
                return False
    
    return True

def main():
    if len(sys.argv) != 4:
        print("Usage: compare_numerical.py <file1> <file2> <tolerance>", file=sys.stderr)
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    tol = float(sys.argv[3])
    
    try:
        with open(file1, 'r') as f1, open(file2, 'r') as f2:
            lines1 = f1.readlines()
            lines2 = f2.readlines()
            
            if len(lines1) != len(lines2):
                sys.exit(1)
            
            for l1, l2 in zip(lines1, lines2):
                if not compare_lines(l1, l2, tol):
                    sys.exit(1)
        
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()

