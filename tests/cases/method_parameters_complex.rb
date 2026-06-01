class MethodParamBox
  def sample(a, b = 1, *rest, key:, opt: 2, **kw, &blk); end
end

m = MethodParamBox.new.method(:sample)
p m.arity
p m.parameters
