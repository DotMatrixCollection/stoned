class ParentForInherited
  @seen = []
  def self.seen; @seen; end
  def self.inherited(sub); @seen << sub.name; end
end

class ChildForInherited < ParentForInherited; end
p ParentForInherited.seen
