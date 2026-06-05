items = ["pear", "fig", "banana", "kiwi"]
p items.sort_by { |s| [s.length, s] }
p items.min_by { |s| s.length }
p items.max_by { |s| s.length }
