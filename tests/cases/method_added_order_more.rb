class HookBox
  @added = []
  def self.added; @added; end
  def self.method_added(name); @added << name; end
  def alpha; end
  def beta; end
end

p HookBox.added
