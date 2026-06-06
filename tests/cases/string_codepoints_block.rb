r = []
"abc".codepoints { |cp| r << cp }
p r
p "abc".codepoints { |cp| cp }
