# case/when with ranges, classes, regex, and values
def grade(score)
  case score
  when 90..100 then "A"
  when 80...90 then "B"
  when 70...80 then "C"
  when 60...70 then "D"
  else              "F"
  end
end

p grade(95)
p grade(85)
p grade(75)
p grade(65)
p grade(55)
p grade(100)
p grade(80)

# case with class matching
def describe(x)
  case x
  when Integer then "integer"
  when Float   then "float"
  when String  then "string"
  when Array   then "array"
  when NilClass then "nil"
  else "unknown"
  end
end

p describe(42)
p describe(3.14)
p describe("hi")
p describe([1,2])
p describe(nil)

# case with regex
def classify_word(w)
  case w
  when /\A\d+\z/  then "digits"
  when /\A[A-Z]/  then "capitalized"
  when /\s/       then "has space"
  else                 "other"
  end
end

p classify_word("123")
p classify_word("Hello")
p classify_word("foo bar")
p classify_word("hello")

# case with no expression uses first truthy when
x = 42
result = case
         when x < 0    then "negative"
         when x == 0   then "zero"
         when x < 100  then "small positive"
         else               "large"
         end
p result
