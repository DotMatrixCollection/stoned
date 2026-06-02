class SuperChainBase
  def value
    "base"
  end
end

class SuperChainMid < SuperChainBase
  def value
    "mid"
  end
end

class SuperChainLeaf < SuperChainMid
  def value
    "leaf"
  end
end

m = SuperChainLeaf.new.method(:value)
p m.call
p m.super_method.call
p m.super_method.super_method.call
p m.super_method.super_method.super_method
