require "forwardable"

class ForwardQueue
  include Forwardable

  def initialize
    @items = ["a", "b"]
  end

  def_delegator :@items, :push, :add
  def_delegator :@items, :length
end

q = ForwardQueue.new
p q.length
p q.add("c")
p q.length
