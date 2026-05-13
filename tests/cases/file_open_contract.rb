path = "/tmp/stoned_file_open_contract.txt"
File.write(path, "abc")

f = File.open(path, nil)
puts f.read.inspect
f.close

[false, true, :r, []].each do |m|
  begin
    File.open(path, m)
  rescue => e
    puts m.inspect
    puts e.class
    puts e.message
  end
end

begin
  File.open(path, "q")
rescue => e
  puts e.class
  puts e.message
end

File.delete(path)
