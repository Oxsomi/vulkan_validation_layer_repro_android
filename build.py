import subprocess
import glob
import os
import shutil

def main():

	# Find directory with aapt, zipalign, apksigner, etc.

	for f in glob.glob(os.environ["ANDROID_SDK"] + "/build-tools/*"):
		buildTools = f
		break

	buildTools += "/"

	# Build with conan

	subprocess.check_output("conan build . --profile \"android_armv8_33_MinGW Makefiles\" -s build_type=Debug")

	for f in glob.glob("build/Debug/*.so"):
		shutil.copy2(f, "lib/arm64-v8a")

	# Make unsigned apk
	
	print("-- Creating apk")
	subprocess.check_output("\"" + buildTools + "aapt\" package -f -I \"" + os.environ["ANDROID_SDK"] + "/platforms/android-33/android.jar\" -M \"AndroidManifest.xml\" -S res -A assets -m -F app-unsigned.apk")
	
	print("-- Adding libs to apk")

	for f in glob.glob("lib/*"):
		for f0 in glob.glob(f + "/*.so"):
			subprocess.check_output("\"" + buildTools + "aapt\" add app-unsigned.apk \"" + f0.replace("\\", "/") + "\"")

	print("-- Zipalign apk")
	subprocess.check_output("\"" + buildTools + "zipalign\" -v -f 4 app-unsigned.apk app.apk")

	apkFile = "app.apk"
	keystore = ".keystore"

	if not os.path.isfile(keystore):
		print("-- Creating keystore")
		subprocess.check_output("\"" + os.environ["JAVA_HOME"] + "/bin/keytool\" -genkeypair -v -keystore \"" + keystore + "\" -keyalg RSA -keysize 2048 -validity 10000")

	print("-- Sign apk, please enter keystore password")
	pwd = str(input())

	signCommand = "\"" + buildTools + "apksigner.bat\"" if os.name == "nt" else "bash \"" + buildTools + "apksigner.sh\""
	subprocess.check_output(signCommand + " sign --ks \"" + keystore + "\" --ks-pass \"pass:" + pwd + "\" --key-pass \"pass:" + pwd + "\" --min-sdk-version 33 app.apk")
	
	print("-- Successfully signed apk")

	adb = os.environ["ANDROID_SDK"] + "/platform-tools/adb"

	print("-- Installing apk file (" + apkFile + ") using adb (" + adb + ")")
	subprocess.check_output(adb + " install -t -r " + apkFile)

	print("-- Running apk file")
	subprocess.check_output(adb + " logcat -c")		# Clear log first
	subprocess.run(adb + " shell am start -n net.osomi.test/android.app.NativeActivity")

if __name__ == "__main__":
	main()