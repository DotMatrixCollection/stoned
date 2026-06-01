class Config
  attr_accessor :debug, :host
  def initialize
    @debug = false
    @host = "localhost"
  end
end

c = Config.new
c.freeze
p c.frozen?

begin
  c.debug = true
rescue => e
  p e.class
end

begin
  c.host = "other"
rescue => e
  p e.class
end

p c.debug
p c.host
