r = []
"a\nb\nc".lines { |l| r << l.chomp }
p r
p "x\ny\n".lines { |l| l }
