require "forwardable"

class WordBox
  extend Forwardable
  def initialize; @word = "hello"; end
  def_delegator :@word, :upcase, :loud
end

p WordBox.new.loud
