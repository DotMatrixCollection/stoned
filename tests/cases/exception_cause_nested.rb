# Basic cause chain
begin
  raise "root error"
rescue => e1
  begin
    raise "wrapper error"
  rescue => e2
    p e2.message
    p e2.cause.message
    p e2.cause.class
  end
end

# Three-level chain
begin
  raise ArgumentError, "level 1"
rescue => a
  begin
    raise TypeError, "level 2"
  rescue => b
    begin
      raise RuntimeError, "level 3"
    rescue => c
      p c.message
      p c.cause.message
      p c.cause.cause.message
      p c.cause.cause.class
    end
  end
end

# cause is nil when raised without active exception
begin
  raise "standalone"
rescue => e
  p e.cause.nil?
end
