values = [1, 4, 7, 10, 13]

p values.bsearch { |n| n >= 8 }
p values.bsearch_index { |n| n >= 8 }
p values.bsearch { |n| n >= 99 }
p values.bsearch_index { |n| n >= 99 }
