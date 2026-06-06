# exception propagates through times + ensure runs
log = []
begin
  3.times do |i|
    begin
      raise "iter #{i}" if i == 1
      log << "ok:#{i}"
    ensure
      log << "ens:#{i}"
    end
  end
rescue => e
  log << "caught: #{e.message}"
end
p log

# exception through upto
log2 = []
begin
  1.upto(4) do |i|
    raise "up:#{i}" if i == 3
    log2 << i
  end
rescue => e
  log2 << "rescued: #{e.message}"
end
p log2

# break value from times
val = 5.times.map { |i| break "stopped at #{i}" if i == 2; i }
p val
