module IncludeHookMore
  @seen = []
  def self.seen; @seen; end
  def self.included(base); @seen << base.name; end
end

class IncludeHostMore; include IncludeHookMore; end
p IncludeHookMore.seen
