items = ["pear", "fig", "banana", "kiwi"]
p items.minmax_by { |s| s.length }
p items.sort_by { |s| [s[-1], s] }
