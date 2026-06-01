class Foo
  def pub; "public"; end
  private
  def priv; "private"; end
  protected
  def prot; "protected"; end
end

f = Foo.new
p f.respond_to?(:pub)
p f.respond_to?(:priv)
p f.respond_to?(:priv, true)
p f.respond_to?(:prot)
p f.respond_to?(:prot, true)
p f.respond_to?(:nonexistent)
p f.respond_to?(:to_s)
p f.respond_to?(:class)
