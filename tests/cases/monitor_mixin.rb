require "monitor"

class Guarded
  include MonitorMixin

  def initialize
    super
    @value = 0
  end

  def inc
    synchronize { @value += 1 }
  end
end

g = Guarded.new
p g.try_enter
g.mon_exit
p g.inc
p g.new_cond.class
