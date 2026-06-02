class PublicSendDynamic
  def alpha(value)
    "alpha:#{value}"
  end

  private

  def hidden
    "hidden"
  end
end

obj = PublicSendDynamic.new
name = "al" + "pha"

p obj.public_send(name, 7)
p obj.respond_to?(name)
p obj.respond_to?(:hidden)

begin
  obj.public_send(:hidden)
rescue NoMethodError => e
  p e.class
end
