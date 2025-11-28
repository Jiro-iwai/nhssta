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

def is_correlation_value(val_str):
    """Check if a value is likely a correlation coefficient (0.0-1.0 range)."""
    try:
        val = float(val_str)
        # Correlation values are typically in [0.0, 1.0] range
        # LAT values are typically larger (e.g., 35.016, 3.575)
        return 0.0 <= abs(val) <= 1.0
    except ValueError:
        return False

def compare_lines(line1, line2, lat_tol, corr_tol, max_errors):
    """Compare two lines with numerical tolerance.
    
    Args:
        line1: First line to compare
        line2: Second line to compare
        lat_tol: Tolerance for LAT values (mean, stddev) - relative error
        corr_tol: Tolerance for correlation values - relative error
        max_errors: Dictionary to track maximum errors {'lat': max_lat_error, 'corr': max_corr_error}
    """
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
    
    # Determine if this is a correlation line (tab-separated, values in [0,1] range)
    # or LAT line (space-separated, larger values)
    is_correlation_line = '\t' in line1 or '\t' in line2
    
    for f1, f2 in zip(fields1, fields2):
        if is_numeric(f1) and is_numeric(f2):
            # Determine tolerance based on value type
            val1 = float(f1)
            val2 = float(f2)
            diff = abs(val1 - val2)
            
            # Calculate relative error
            rel_err = 0.0
            if abs(val1) > 1e-10 and abs(val2) > 1e-10:
                rel_err = diff / max(abs(val1), abs(val2))
            
            # Use correlation tolerance if:
            # 1. Line contains tabs (correlation matrix format)
            # 2. Both values are in [0, 1] range (correlation coefficients)
            if is_correlation_line or (is_correlation_value(f1) and is_correlation_value(f2)):
                tol = corr_tol
                # Update maximum correlation error
                if rel_err > max_errors['corr']:
                    max_errors['corr'] = rel_err
            else:
                # LAT values (mean, stddev)
                tol = lat_tol
                # Update maximum LAT error
                if rel_err > max_errors['lat']:
                    max_errors['lat'] = rel_err
            
            if not compare_values(f1, f2, tol):
                return False
        else:
            if f1 != f2:
                return False
    
    return True

def main():
    if len(sys.argv) < 4:
        print("Usage: compare_numerical.py <file1> <file2> <lat_tolerance> [corr_tolerance]", file=sys.stderr)
        print("  lat_tolerance: Tolerance for LAT values (mean, stddev) - relative error (default: 0.002 = 0.2%)", file=sys.stderr)
        print("  corr_tolerance: Tolerance for correlation values - relative error (default: 0.032 = 3.2%)", file=sys.stderr)
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    lat_tol = float(sys.argv[3])
    corr_tol = float(sys.argv[4]) if len(sys.argv) > 4 else 0.032  # Default 3.2%
    
    # Track maximum errors
    max_errors = {'lat': 0.0, 'corr': 0.0}
    all_passed = True
    
    try:
        with open(file1, 'r') as f1, open(file2, 'r') as f2:
            lines1 = f1.readlines()
            lines2 = f2.readlines()
            
            if len(lines1) != len(lines2):
                print(f"MAX_LAT_ERROR:N/A MAX_CORR_ERROR:N/A (line count mismatch: {len(lines1)} vs {len(lines2)})")
                sys.exit(1)
            
            for l1, l2 in zip(lines1, lines2):
                if not compare_lines(l1, l2, lat_tol, corr_tol, max_errors):
                    all_passed = False
                    # Continue to collect all errors instead of exiting early
        
        # Always output maximum errors to stdout (for parsing by shell script)
        # Format: MAX_LAT_ERROR:0.001234 MAX_CORR_ERROR:0.023456
        print(f"MAX_LAT_ERROR:{max_errors['lat']:.6f} MAX_CORR_ERROR:{max_errors['corr']:.6f}")
        sys.exit(0 if all_passed else 1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()

