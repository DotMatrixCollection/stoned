class ClassBase
  def self.label(value = "base")
    "base:#{value}"
  end
end

class ClassChild < ClassBase
  def self.label(value = "child")
    super(value).upcase
  end
end

class ClassGrandchild < ClassChild
  def self.label
    super("grand") + "!"
  end
end

p ClassChild.label
p ClassGrandchild.label
p ClassGrandchild.method(:label).call
