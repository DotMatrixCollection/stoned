# ensure always runs regardless of exception or normal exit
def with_ensure
  result = []
  begin
    result << "try"
    result << "end"
  ensure
    result << "ensure"
  end
  result
end
p with_ensure

# ensure runs even when exception is raised and rescued
def rescued_ensure
  log = []
  begin
    log << "before"
    raise "oops"
  rescue => e
    log << "rescued:#{e.message}"
  ensure
    log << "ensure"
  end
  log
end
p rescued_ensure

# ensure runs when exception propagates
def propagating_ensure
  log = []
  begin
    raise "boom"
  ensure
    log << "ensure ran"
  end
rescue
  log << "outer rescue"
  log
end
p propagating_ensure

# else clause runs when no exception raised
def with_else
  log = []
  begin
    log << "try"
  rescue
    log << "rescue"
  else
    log << "else"
  ensure
    log << "ensure"
  end
  log
end
p with_else

# else skipped when exception raised
def else_skipped
  log = []
  begin
    raise "err"
  rescue
    log << "rescue"
  else
    log << "else"
  ensure
    log << "ensure"
  end
  log
end
p else_skipped
