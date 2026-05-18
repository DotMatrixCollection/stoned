$stdout.sync = true

require "bundler/friendly_errors"

value = Bundler::FriendlyErrors.with_friendly_errors do
  42
end
puts value

ran = false
Bundler::FriendlyErrors.with_friendly_errors do
  ran = true
end
puts ran
