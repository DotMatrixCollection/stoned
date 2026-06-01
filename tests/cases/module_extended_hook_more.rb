module ExtendHookMore
  @seen = []
  def self.seen; @seen; end
  def self.extended(obj); @seen << obj.class.to_s; end
end

Object.new.extend(ExtendHookMore)
p ExtendHookMore.seen
