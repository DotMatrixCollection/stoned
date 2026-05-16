def capture(*args)
  puts args.length
  puts args[1][:external_encoding]
  puts args[1][:internal_encoding]
end

capture(1, :external_encoding => "utf-8", :internal_encoding => "-")
