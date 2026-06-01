require "forwardable"

class ForwardBox
  extend Forwardable

  def initialize
    @ary = [1, 2]
  end

  def_delegator :@ary, :length, :size
  def_delegators :@ary, :first, :last
end

b = ForwardBox.new
p b.size
p b.first
p b.last
