# Bare method call in case/when body
def double(x); x * 2; end
def greet; "hello"; end

result = case 5
when 1, 2 then "small"
when 3..7 then double(5)
else "big"
end
p result

p(case "x"
when "x" then greet
end)

# Private method in class
class WordParser
  def run(input)
    case input[0]
    when '0'..'9' then parse_num(input)
    when 'a'..'z' then parse_word(input)
    else nil
    end
  end
  private
  def parse_num(s); s.to_i; end
  def parse_word(s); s.upcase; end
end

wp = WordParser.new
p wp.run("42")
p wp.run("hello")
p wp.run("!")
