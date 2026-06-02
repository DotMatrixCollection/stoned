class Prefixer
  def initialize(prefix)
    @prefix = prefix
  end

  def call(value)
    "#{@prefix}:#{value}"
  end
end

prefixer = Prefixer.new("tag")
mapper = prefixer.method(:call).to_proc

p [1, 2, 3].map(&mapper)
p mapper.call("x")
p prefixer.method(:call).receiver == prefixer
