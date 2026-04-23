class DefinedBox
  def initialize
    @v = 1
  end

  def ping
    :pong
  end

  def check(box)
    local = 1

    puts(defined?(local).inspect)
    puts(defined?(missing).inspect)
    puts(defined?(@v).inspect)
    puts(defined?(@missing).inspect)
    puts(defined?(self).inspect)
    puts(defined?(nil).inspect)
    puts(defined?(true).inspect)
    puts(defined?(false).inspect)
    puts(defined?(Integer).inspect)
    puts(defined?(MissingConstant).inspect)
    puts(defined?(ping).inspect)
    puts(defined?(missing_method).inspect)
    puts(defined?(box.ping).inspect)
    puts(defined?(box.missing).inspect)
    puts(defined?(1 + 2).inspect)
  end
end

box = DefinedBox.new
box.check(box)
