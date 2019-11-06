#!/usr/bin/env python
# Convert Hyg star database from CSV to JSON
# Paul Melis <paul.melis@surfsara.nl>
import sys, csv, json

STRING_FIELDS = {
    # v3
    'proper', 'gl', 'spect', 'con', 'var', 'bf', 'bayer', 'base',
    # v2
    'Spectrum', 'Gliese', 'BayerFlamsteed', 'ProperName'
}

lst = []

with open(sys.argv[1], 'rt', newline='') as f:
    
    r = csv.reader(f, delimiter=',')
    
    columns = next(r)
    
    for row in r:
        #print(row)
        
        entry = {}
        
        for key, value in zip(columns,row):
            
            if key not in STRING_FIELDS:
            
                if value == '':
                    value = None
                else:
                    try:
                        value = int(value)
                    except ValueError:
                        try:
                            value = float(value)
                        except ValueError:
                            print("Can't convert non-string value for key %s" % key)
                        
            entry[key] = value
            
        lst.append(entry)

        
with open(sys.argv[2], 'wt') as g:
    j = json.dumps(lst)
    #print(j)
    g.write(j)
