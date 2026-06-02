module Taggable
  def tag
    @tag ||= "untagged"
  end

  def tag=(value)
    @tag = value
  end
end

item = Object.new
p item.respond_to?(:tag)
item.extend(Taggable)
p item.respond_to?(:tag)
p item.singleton_class.ancestors.include?(Taggable)
p item.tag
item.tag = "ready"
p item.tag
