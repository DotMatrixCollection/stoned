class AliasMore
  def one; 1; end
  alias two one
end

p AliasMore.new.two
p AliasMore.instance_methods.include?(:two)
