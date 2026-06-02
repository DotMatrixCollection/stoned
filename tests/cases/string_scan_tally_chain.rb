text = "one=1 two=2 three=3"
pairs = text.scan(/(\w+)=(\d+)/).map { |k, v| [k, v.to_i] }
p pairs
p pairs.to_h

freq = "hello world hello ruby world hello".split.tally
p freq.max_by { |_, c| c }
p freq.sort_by { |w, _| w }
