OUTER_CONST_MORE = :top

module ConstOuterMore
  OUTER_CONST_MORE = :inner
  class Child
    def self.value
      OUTER_CONST_MORE
    end
  end
end

p ConstOuterMore::Child.value
p ::OUTER_CONST_MORE
