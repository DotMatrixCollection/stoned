$stdout.sync = true

class DslProbe
  attr_reader :calls

  def initialize
    @calls = []
  end

  def source(value)
    @calls << "source:" + value
  end

  def gem(name)
    @calls << "gem:" + name
  end
end

prefix = "rubygems"
dsl = DslProbe.new

probe = proc do
  source prefix.upcase
  gem "demo"
end

dsl.instance_eval(&probe)
puts dsl.calls.join(",")
