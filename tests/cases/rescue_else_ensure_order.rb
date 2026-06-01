order = []
begin
  order << :body
rescue
  order << :rescue
else
  order << :else
ensure
  order << :ensure
end
p order
