module PrependHookMore
  @seen = []
  def self.seen; @seen; end
  def self.prepended(base); @seen << base.name; end
end

class PrependHostMore; prepend PrependHookMore; end
p PrependHookMore.seen
